#include <klibc/string.h>
#include <lunaix/fs.h>
#include <lunaix/fs/iso9660.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

static struct v_inode_ops iso_inode_ops = {
    .dir_lookup = iso9660_dir_lookup,
    .open = iso9660_open,
};

static struct v_file_ops iso_file_ops = { .close = iso9660_close,
                                          .read = iso9660_read,
                                          .write = iso9660_write,
                                          .seek = iso9660_seek,
                                          .readdir = iso9660_readdir };

void
iso9660_init_inode(struct v_superblock* vsb, struct v_inode* inode)
{
    inode->data = vzalloc(sizeof(struct iso_inode));
}

int
iso9660_fill_inode(struct v_inode* inode, struct iso_drecache* dir, int ino)
{
    int errno = 0;
    struct device* dev = inode->sb->dev;
    struct iso_inode* isoino = (struct iso_inode*)inode->data;

    // In the spec, there is a differentiation in how file section organized
    // between interleaving and non-interleaving mode. To simplify this, we
    // treat the non-interleaving as an interleaving with gap size = 0 and fu
    // size = 1
    isoino->fu_size = dir->fu_size;
    isoino->gap_size = dir->gap_size;

    u32_t fu_len = isoino->fu_size * ISO9660_BLKSZ;

    inode->id = ino;
    inode->lb_addr = dir->extent_addr;
    inode->ops = &iso_inode_ops;
    inode->default_fops = &iso_file_ops;

    // xattr_len is in unit of FU. Each FU comprise <fu_sz> block(s).
    inode->fsize = dir->data_size - dir->xattr_len * fu_len;

    if ((dir->flags & ISO_FDIR)) {
        inode->itype = VFS_IFDIR;
    } else {
        inode->itype = VFS_IFFILE;
    }

    if (dir->xattr_len) {
        struct iso_xattr* xattr = (struct iso_xattr*)valloc(ISO9660_BLKSZ);
        // Only bring in single FU, as we only care about the attributes.
        errno =
          dev->read(dev, xattr, ISO9660_BLKSZ * inode->lb_addr, ISO9660_BLKSZ);
        if (errno < 0) {
            return EIO;
        }
        isoino->record_fmt = xattr->record_fmt;
        isoino->ctime = iso9660_dt2unix(&xattr->ctime);
        isoino->mtime = iso9660_dt2unix(&xattr->mtime);

        inode->lb_addr += dir->xattr_len * dir->fu_size;

        vfree(xattr);
    }

    return 0;
}