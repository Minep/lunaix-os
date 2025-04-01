#include <lunaix/device.h>
#include <lunaix/fs/api.h>
#include <lunaix/fs/devfs.h>
#include <lunaix/spike.h>

#include <usr/lunaix/dirent_defs.h>

extern struct v_inode_ops devfs_inode_ops;
extern struct v_file_ops devfs_file_ops;

int
devfs_read(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    assert(inode->data);

    struct device* dev = resolve_device(inode->data);

    if (!dev || !dev->ops.read) {
        return ENOTSUP;
    }

    return dev->ops.read(dev, buffer, fpos, len);
}

int
devfs_write(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    assert(inode->data);

    struct device* dev = resolve_device(inode->data);

    if (!dev || !dev->ops.write) {
        return ENOTSUP;
    }

    return dev->ops.write(dev, buffer, fpos, len);
}

int
devfs_read_page(struct v_inode* inode, void* buffer, size_t fpos)
{
    assert(inode->data);

    struct device* dev = resolve_device(inode->data);

    if (!dev || !dev->ops.read_page) {
        return ENOTSUP;
    }

    return dev->ops.read_page(dev, buffer, fpos);
}

int
devfs_write_page(struct v_inode* inode, void* buffer, size_t fpos)
{
    assert(inode->data);

    struct device* dev = resolve_device(inode->data);

    if (!dev || !dev->ops.read_page) {
        return ENOTSUP;
    }

    return dev->ops.read_page(dev, buffer, fpos);
}

int
devfs_get_itype(morph_t* obj)
{
    int itype = VFS_IFDEV;

    if (morph_type_of(obj, devcat_morpher)) {
        return VFS_IFDIR;
    }

    struct device* dev = resolve_device(obj);

    if (!dev) {
        return itype;
    }
    
    int dev_if = dev->dev_type & DEV_MSKIF;
    if (dev_if == DEV_IFVOL) {
        itype |= VFS_IFVOLDEV;
    }

    // otherwise, the mapping is considered to be generic seq dev.
    return itype;
}

static inline int
devfs_get_dtype(morph_t* dev_morph)
{
    if (morph_type_of(dev_morph, devcat_morpher)) {
        return DT_DIR;
    }
    return DT_FILE;
}

static inline morph_t*
__try_resolve(struct v_inode* inode)
{
    if (!inode->data) {
        return dev_object_root;
    }

    return resolve_device_morph(inode->data);
}

int
devfs_mknod(struct v_dnode* dnode, morph_t* obj)
{
    struct v_inode* devnod;
    
    assert(obj);

    devnod = vfs_i_find(dnode->super_block, morpher_uid(obj));
    if (!devnod) {
        if ((devnod = vfs_i_alloc(dnode->super_block))) {
            devnod->id = morpher_uid(obj);
            devnod->data = changeling_ref(obj);
            devnod->itype = devfs_get_itype(obj);

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
    morph_t *mobj, *root;

    root = __try_resolve(this);
    if (!root) {
        return ENOTDIR;
    }

    mobj = changeling_find(root, &dnode->name);
    if (!mobj) {
        return ENOENT;
    }

    return devfs_mknod(dnode, mobj);
}

int
devfs_readdir(struct v_file* file, struct dir_context* dctx)
{
    morph_t *mobj, *root;

    root = __try_resolve(file->inode);
    if (!root) {
        return ENOTDIR;
    }

    if (fsapi_handle_pseudo_dirent(file, dctx)) {
        return 1;
    }

    mobj = changeling_get_at(root, file->f_pos - 2);
    if (!mobj) {
        return 0;
    }

    dctx->read_complete_callback(dctx, 
                                 mobj->name.value, mobj->name.len, 
                                 devfs_get_dtype(mobj));
    return 1;
}

void
devfs_init_inode(struct v_superblock* vsb, struct v_inode* inode)
{
    inode->ops = &devfs_inode_ops;
    inode->default_fops = &devfs_file_ops;

    // we set default access right to be 0775.
    // TODO need a way to allow this to be changed
    
    fsapi_inode_setaccess(inode, FSACL_DEFAULT);
    fsapi_inode_setowner(inode, 0, 0);
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
    struct filesystem* fs;
    fs = fsapi_fs_declare("devfs", FSTYPE_PSEUDO);
    
    fsapi_fs_set_mntops(fs, devfs_mount, devfs_unmount);
    fsapi_fs_finalise(fs);
}
EXPORT_FILE_SYSTEM(devfs, devfs_init);

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