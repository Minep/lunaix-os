#include <lunaix/fs/api.h>
#include <lunaix/block.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/page.h>
#include <lunaix/syslog.h>

#include "ext2.h"

LOG_MODULE("EXT2")

#define EXT2_COMPRESSION    0x0001
#define EXT2_FILETYPE       0x0002
#define EXT2_JOURNAL        0x0004
#define EXT2_METABG         0x0008

#define EXT2_SPARSESB       0x0001
#define EXT2_LARGEFLE       0x0002
#define EXT2_BTREEDIR       0x0004

#define EXT2_SUPER_MAGIC            0xef53
#define EXT2_BASE_BLKSZ             1024
#define EXT2_PRIME_SB_OFF           EXT2_BASE_BLKSZ

// current support for incompatible features
#define EXT2_IMPL_REQFEAT           (EXT2_FILETYPE)

// current support for readonly feature
#define EXT2_IMPL_ROFEAT            (EXT2_SPARSESB)

#define EXT2_ROOT_INO               to_ext2ino_id(1)

#define check_compat_mnt(feat) \
        (!((feat) & ~EXT2_IMPL_REQFEAT))

#define check_compat_mnt_ro_fallback(feat) \
        (((feat) & ~EXT2_IMPL_ROFEAT))

static size_t
ext2_rd_capacity(struct v_superblock* vsb)
{
    struct ext2_sbinfo* sb = fsapi_impl_data(vsb, struct ext2_sbinfo);
    return sb->raw->s_blk_cnt * fsapi_block_size(vsb);
}

static void
__vsb_release(struct v_superblock* vsb)
{
    ext2gd_release_gdt(vsb);
    vfree(vsb->data);
}

static size_t
ext2_rd_usage(struct v_superblock* vsb)
{
    struct ext2_sbinfo* sb = fsapi_impl_data(vsb, struct ext2_sbinfo);
    size_t used = sb->raw->s_free_blk_cnt - sb->raw->s_blk_cnt;
    return used * fsapi_block_size(vsb);
}

struct fsapi_vsb_ops vsb_ops = {
    .read_capacity = ext2_rd_capacity,
    .read_usage = ext2_rd_usage,
    .init_inode = ext2ino_init,
    .release = __vsb_release
};

static inline unsigned int
__translate_feature(struct ext2b_super* sb)
{
    unsigned int feature = 0;
    unsigned int req, opt, ro;

    req = sb->s_required_feat;
    opt = sb->s_optional_feat;
    ro  = sb->s_ro_feat;

    if ((req & EXT2_COMPRESSION)) {
        feature |= FEAT_COMPRESSION;
    }

    if ((req & EXT2_FILETYPE)) {
        feature |= FEAT_FILETYPE;
    }

    if ((ro & EXT2_SPARSESB)) {
        feature |= FEAT_SPARSE_SB;
    }

    if ((ro & EXT2_LARGEFLE)) {
        feature |= FEAT_LARGE_FILE;
    }

    return feature;
}

static bool
__check_mount(struct v_superblock* vsb, struct ext2b_super* sb)
{
    unsigned int req, opt, ro;

    req = sb->s_required_feat;
    opt = sb->s_optional_feat;
    ro  = sb->s_ro_feat;
    
    if (sb->s_magic != EXT2_SUPER_MAGIC) {
        ERROR("invalid magic: 0x%x", sb->s_magic);
        return false;
    }

    if (!check_compat_mnt(req)) 
    {
        ERROR("unsupported feature: 0x%x, mount refused", req);
        return false;
    }

    if (check_compat_mnt_ro_fallback(ro)) 
    {
        WARN("unsupported feature: 0x%x, mounted as readonly", ro);
        fsapi_set_readonly_mount(vsb);
    }

#ifndef CONFIG_ARCH_BITS_64
    if ((ro & EXT2_LARGEFLE)) {
        WARN("large file not supported on 32bits machine");
        fsapi_set_readonly_mount(vsb);
    }
#endif

    return true;
}

static int 
ext2_mount(struct v_superblock* vsb, struct v_dnode* mnt)
{
    struct device* bdev;
    struct ext2_sbinfo* ext2sb;
    struct ext2b_super* rawsb;
    struct v_inode* root_inode;
    bbuf_t buf;
    size_t block_size;
    int errno = 0;
    unsigned int req_feat;

    bdev = fsapi_blockdev(vsb);
    ext2sb = vzalloc(sizeof(*ext2sb));
    rawsb = vzalloc(sizeof(*rawsb));

    errno = bdev->ops.read(bdev, rawsb, EXT2_PRIME_SB_OFF, sizeof(*rawsb));
    if (errno < 0) {
        goto failed;
    }

    block_size = EXT2_BASE_BLKSZ << rawsb->s_log_blk_size;
    fsapi_begin_vsb_setup(vsb, block_size);
    
    if (!__check_mount(vsb, rawsb)) {
        goto unsupported;
    }

    if (block_size > PAGE_SIZE) {
        ERROR("block size must not greater than page size");
        errno = EINVAL;
        goto failed;
    }

    ext2sb->bdev = bdev;
    ext2sb->block_size = block_size;
    ext2sb->vsb = vsb;
    ext2sb->read_only = fsapi_readonly_mount(vsb);
    ext2sb->raw = rawsb;
    ext2sb->all_feature = __translate_feature(rawsb);

    fsapi_set_vsb_ops(vsb, &vsb_ops);
    fsapi_complete_vsb_setup(vsb, ext2sb);

    ext2gd_prepare_gdt(vsb);

    root_inode = vfs_i_alloc(vsb);
    ext2ino_fill(root_inode, EXT2_ROOT_INO);
    vfs_assign_inode(mnt, root_inode);

    // replace the superblock raw buffer with bcache managed one
    buf = fsblock_get(vsb, ext2_datablock(vsb, 0));
    if (block_size == EXT2_BASE_BLKSZ) {
        ext2sb->raw = blkbuf_data(buf);
    }
    else {
        ext2sb->raw = offset(blkbuf_data(buf), EXT2_BASE_BLKSZ);
    }

    ext2sb->buf = buf;
    vfree(rawsb);
    return 0;

unsupported:
    errno = ENOTSUP;

failed:
    vfree(ext2sb);
    vfree(rawsb);
    fsapi_reset_vsb(vsb);
    return errno;
}

static int 
ext2_umount(struct v_superblock* vsb)
{
    // sync all dirty buffers
    if (!blkbuf_syncall(vsb->blks, false)) {
        return EAGAIN;
    }

    ext2gd_release_gdt(vsb);

    blkbuf_release(vsb->blks);
    return 0;
}

static void
ext2_init()
{
    struct filesystem* fs;
    fs = fsapi_fs_declare("ext2", 0);

    fsapi_fs_set_mntops(fs, ext2_mount, ext2_umount);
    fsapi_fs_finalise(fs);

    gdesc_bcache_zone = bcache_create_zone("ext2_gdesc");
}
EXPORT_FILE_SYSTEM(ext2fs, ext2_init);