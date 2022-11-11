#include <lunaix/device.h>
#include <lunaix/dirent.h>
#include <lunaix/fs.h>
#include <lunaix/fs/devfs.h>
#include <lunaix/spike.h>

extern struct v_inode_ops devfs_inode_ops;
extern struct v_file_ops devfs_file_ops;

int
devfs_read(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    assert(inode->data);

    struct device* dev = (struct device*)inode->data;

    if (!dev->read) {
        return ENOTSUP;
    }

    return dev->read(dev, buffer, fpos, len);
}

int
devfs_write(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    assert(inode->data);

    struct device* dev = (struct device*)inode->data;

    if (!dev->write) {
        return ENOTSUP;
    }

    return dev->write(dev, buffer, fpos, len);
}

int
devfs_read_page(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    assert(inode->data);

    struct device* dev = (struct device*)inode->data;

    if (!dev->read_page) {
        return ENOTSUP;
    }

    return dev->read_page(dev, buffer, fpos);
}

int
devfs_write_page(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    assert(inode->data);

    struct device* dev = (struct device*)inode->data;

    if (!dev->read_page) {
        return ENOTSUP;
    }

    return dev->read_page(dev, buffer, fpos);
}

int
devfs_get_itype(struct device* dev)
{
    int itype = VFS_IFFILE;
    int dev_if = dev->dev_type & DEV_MSKIF;
    if (dev_if == DEV_IFCAT) {
        itype = VFS_IFDIR;
    } else if (dev_if == DEV_IFVOL) {
        itype |= VFS_IFVOLDEV;
    } else {
        itype |= VFS_IFSEQDEV;
    }
    return itype;
}

int
devfs_get_dtype(struct device* dev)
{
    switch (dev->dev_type & DEV_MSKIF) {
        case DEV_IFCAT:
            return DT_DIR;

        default:
            return DT_FILE;
    }
}

int
devfs_mknod(struct v_dnode* dnode, struct device* dev)
{
    assert(dev);

    struct v_inode* devnod = vfs_i_find(dnode->super_block, dev->dev_id);
    if (!devnod) {
        if ((devnod = vfs_i_alloc(dnode->super_block))) {
            devnod->id = dev->dev_id;
            devnod->data = dev;
            devnod->itype = devfs_get_itype(dev);

            vfs_i_addhash(devnod);
        } else {
            return ENOMEM;
        }
    }

    vfs_assign_inode(dnode, devnod);
    return 0;
}

int
devfs_dirlookup(struct v_inode* this, struct v_dnode* dnode)
{
    struct device* dev =
      device_getbyhname((struct device*)this->data, &dnode->name);
    if (!dev) {
        return ENOENT;
    }
    return devfs_mknod(dnode, dev);
}

int
devfs_readdir(struct v_file* file, struct dir_context* dctx)
{
    struct device* dev =
      device_getbyoffset((struct device*)(file->inode->data), dctx->index);
    if (!dev) {
        return 0;
    }
    dctx->read_complete_callback(
      dctx, dev->name.value, dev->name.len, devfs_get_dtype(dev));
    return 1;
}

void
devfs_init_inode(struct v_superblock* vsb, struct v_inode* inode)
{
    inode->ops = &devfs_inode_ops;
    inode->default_fops = &devfs_file_ops;
}

int
devfs_mount(struct v_superblock* vsb, struct v_dnode* mount_point)
{
    vsb->ops.init_inode = devfs_init_inode;

    struct v_inode* rootnod = vfs_i_alloc(vsb);

    if (!rootnod) {
        return ENOMEM;
    }

    rootnod->id = -1;
    rootnod->itype = VFS_IFDIR;

    vfs_i_addhash(rootnod);
    vfs_assign_inode(mount_point, rootnod);

    return 0;
}

int
devfs_unmount(struct v_superblock* vsb)
{
    return 0;
}

void
devfs_init()
{
    struct filesystem* fs = fsm_new_fs("devfs", 5);
    fsm_register(fs);
    fs->mount = devfs_mount;
    fs->unmount = devfs_unmount;
}

struct v_inode_ops devfs_inode_ops = { .dir_lookup = devfs_dirlookup,
                                       .open = default_inode_open,
                                       .mkdir = default_inode_mkdir,
                                       .rmdir = default_inode_rmdir };

struct v_file_ops devfs_file_ops = { .close = default_file_close,
                                     .read = devfs_read,
                                     .read_page = devfs_read_page,
                                     .write = devfs_write,
                                     .write_page = devfs_write_page,
                                     .seek = default_file_seek,
                                     .readdir = devfs_readdir };