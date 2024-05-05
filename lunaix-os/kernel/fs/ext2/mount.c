#include <lunaix/fs/api.h>
#include <lunaix/block.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/page.h>
#include <lunaix/syslog.h>

#include "ext2.h"

LOG_MODULE("EXT2")

#define EXT2_SUPER_MAGIC 0xef53
#define EXT2_BASE_BLKSZ 1024
#define EXT2_PRIME_SB_OFF EXT2_BASE_BLKSZ

#define EXT2_REQFEAT_COMPRESSION 0x0001
#define EXT2_REQFEAT_FILETYPE    0x0002
#define EXT2_REQFEAT_JOURNAL     0x0004
#define EXT2_REQFEAT_METABG      0x0008

#define EXT2_ROFEAT_SPARSESB    0x0001
#define EXT2_ROFEAT_LARGEFLE    0x0002
#define EXT2_ROFEAT_BTREEDIR    0x0004

#define EXT2_IMPL_REQFEAT (EXT2_REQFEAT_FILETYPE)
#define EXT2_IMPL_ROFEAT (EXT2_ROFEAT_SPARSESB)

#define EXT2_ROOT_INO 1

#define compatible_mount(feat) \
        (((feat) | EXT2_IMPL_REQFEAT) == EXT2_IMPL_REQFEAT)

#define readonly_mount(feat) \
        (((feat) | EXT2_IMPL_ROFEAT) != EXT2_IMPL_ROFEAT)

static size_t
ext2_rd_capacity(struct v_superblock* vsb)
{
    struct ext2_sbinfo* sb = fsapi_impl_data(vsb, struct ext2_sbinfo);
    return sb->raw.s_blk_cnt * fsapi_block_size(vsb);
}

static size_t
ext2_rd_usage(struct v_superblock* vsb)
{
    struct ext2_sbinfo* sb = fsapi_impl_data(vsb, struct ext2_sbinfo);
    size_t used = sb->raw.s_free_blk_cnt - sb->raw.s_blk_cnt;
    return used * fsapi_block_size(vsb);
}

struct fsapi_vsb_ops vsb_ops = {
    .read_capacity = ext2_rd_capacity,
    .read_usage = ext2_rd_usage,
    .init_inode = ext2_init_inode
};

static int 
ext2_mount(struct v_superblock* vsb, struct v_dnode* mnt)
{
    struct device* bdev;
    struct ext2_sbinfo* ext2sb;
    struct ext2b_super* rawsb;
    struct v_inode* root_inode;
    int errno = 0;

    bdev = fsapi_blockdev(vsb);
    ext2sb = vzalloc(sizeof(*ext2sb));
    rawsb = &ext2sb->raw;

    errno = bdev->ops.read(bdev, rawsb, EXT2_PRIME_SB_OFF, sizeof(*rawsb));
    if (errno < 0) {
        goto failed;
    }

    if (rawsb->s_magic != EXT2_SUPER_MAGIC) {
        goto unsupported;
    }

    if (!compatible_mount(rawsb->s_required_feat)) {
        ERROR("unsupported feature, mount refused");
        goto unsupported;
    }

    if (readonly_mount(rawsb->s_required_feat)) {
        WARN("unsupported feature, mounted as readonly");
        fsapi_set_readonly_mount(vsb);
    }

    size_t block_size = EXT2_BASE_BLKSZ << rawsb->s_log_blk_size;

    if (block_size > PAGE_SIZE) {
        ERROR("block size must not greater than page size");
        goto unsupported;
    }

    ext2sb->bdev = bdev;
    ext2sb->block_size = block_size;
    ext2sb->vsb = vsb;
    ext2sb->read_only = fsapi_readonly_mount(vsb);

    fsapi_set_block_size(vsb, block_size);
    fsapi_set_vsb_ops(vsb, &vsb_ops);
    fsapi_complete_vsb(vsb, ext2sb);

    ext2gd_prepare_gdt(vsb);
    
    root_inode = vfs_i_alloc(vsb);
    ext2_fill_inode(root_inode, EXT2_ROOT_INO);
    vfs_assign_inode(mnt, root_inode);

    return 0;

unsupported:
    errno = ENOTSUP;

failed:
    return errno;
}