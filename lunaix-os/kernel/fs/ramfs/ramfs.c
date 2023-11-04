/**
 * @file ramfs.c
 * @author Lunaixsky
 * @brief RamFS - a file system sit in RAM
 * @version 0.1
 * @date 2022-08-18
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <klibc/string.h>
#include <lunaix/fs.h>
#include <lunaix/fs/ramfs.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

/*
    A RAM FS will play a role of being a root.

    This is an temporary meaure as we don't have any concrete fs
    yet. In the near future, we will have an ISO-9660 as our first
    fs and mount our boot medium as root. And similar thing could
    be done when we have installed our OS into hard disk, in that
    case, our rootfs will be something like ext2.

    RamFS vs. TwiFS: Indeed, they are both fs that lives in RAM so
    there is no foundmental differences. However, TwiFS is designed
    to be a 'virtual FIlesystem for KERnel space' (FIKER), so other
    kernel sub-modules can just create node and attach their own
    implementation of read/write, without brothering to create a
    full featured concrete filesystem. This particularly useful for
    kernel state exposure. In Lunaix, TwiFS is summation of procfs,
    sysfs and kernfs in Linux world.

    However, there is a smell of bad-design in the concept of TwiFS.
    Since it tries to integrate too much responsibility. The TwiFS may
    be replaced by more specific fs in the future.

    On the other hand, RamFS is designed to be act like a dummy fs
    that just ensure 'something there at least' and thus provide basic
    'mountibility' for other fs.
*/

static volatile inode_t ino = 0;

extern const struct v_inode_ops ramfs_inode_ops;
extern const struct v_file_ops ramfs_file_ops;

static int
__ramfs_mknod(struct v_dnode* dnode, struct v_inode** nod_out, u32_t flags)
{
    struct v_inode* inode = vfs_i_alloc(dnode->super_block);

    if (!inode) {
        return ENOMEM;
    }

    struct ram_inode* rinode = valloc(sizeof(struct ram_inode));

    if (!rinode) {
        vfs_i_free(inode);
        return ENOMEM;
    }

    rinode->flags = flags;
    inode->data = rinode;

    if (!(flags & RAMF_DIR)) {
        inode->itype = VFS_IFFILE;
    }

    if ((flags & RAMF_SYMLINK)) {
        inode->itype |= VFS_IFSYMLINK;
    }

    if (nod_out) {
        *nod_out = inode;
    }

    vfs_i_addhash(inode);
    vfs_assign_inode(dnode, inode);

    return 0;
}

int
ramfs_readdir(struct v_file* file, struct dir_context* dctx)
{
    int i = 0;
    struct v_dnode *pos, *n;
    llist_for_each(pos, n, &file->dnode->children, siblings)
    {
        if (i++ >= dctx->index) {
            dctx->read_complete_callback(dctx,
                                         pos->name.value,
                                         pos->name.len,
                                         vfs_get_dtype(pos->inode->itype));
            return 1;
        }
    }
    return 0;
}

int
ramfs_mkdir(struct v_inode* this, struct v_dnode* dnode)
{
    return __ramfs_mknod(dnode, NULL, RAMF_DIR);
}

int
ramfs_create(struct v_inode* this, struct v_dnode* dnode)
{
    return __ramfs_mknod(dnode, NULL, RAMF_FILE);
}

void
ramfs_inode_init(struct v_superblock* vsb, struct v_inode* inode)
{
    inode->id = ino++;
    inode->ops = (struct v_inode_ops*)&ramfs_inode_ops;
    inode->default_fops = (struct v_file_ops*)&ramfs_file_ops;
}

int
ramfs_mount(struct v_superblock* vsb, struct v_dnode* mount_point)
{
    vsb->ops.init_inode = ramfs_inode_init;

    return __ramfs_mknod(mount_point, NULL, RAMF_DIR);
}

int
ramfs_unmount(struct v_superblock* vsb)
{
    return 0;
}

void
ramfs_init()
{
    struct filesystem* ramfs = fsm_new_fs("ramfs", -1);
    ramfs->mount = ramfs_mount;
    ramfs->unmount = ramfs_unmount;

    fsm_register(ramfs);
}
EXPORT_FILE_SYSTEM(ramfs, ramfs_init);

int
ramfs_mksymlink(struct v_inode* this, const char* target)
{
    struct ram_inode* rinode = RAM_INODE(this->data);

    assert(!(rinode->flags & RAMF_SYMLINK));

    size_t len = strlen(target);
    char* symlink = valloc(len);

    if (!symlink) {
        return ENOMEM;
    }

    memcpy(symlink, target, len);

    this->itype |= VFS_IFSYMLINK;
    rinode->flags |= RAMF_SYMLINK;
    rinode->symlink = symlink;

    return 0;
}

int
ramfs_read_symlink(struct v_inode* this, const char** path_out)
{
    struct ram_inode* rinode = RAM_INODE(this->data);

    if (!(rinode->flags & RAMF_SYMLINK)) {
        return EINVAL;
    }

    *path_out = rinode->symlink;

    return 0;
}

int
ramfs_unlink(struct v_inode* this)
{
    struct ram_inode* rinode = RAM_INODE(this->data);

    if ((rinode->flags & RAMF_SYMLINK)) {
        rinode->flags &= ~RAMF_SYMLINK;
        this->itype &= ~VFS_IFSYMLINK;

        vfree(rinode->symlink);

        return 0;
    }

    // TODO

    return 0;
}

const struct v_inode_ops ramfs_inode_ops = { .mkdir = ramfs_mkdir,
                                             .rmdir = default_inode_rmdir,
                                             .dir_lookup =
                                               default_inode_dirlookup,
                                             .create = ramfs_create,
                                             .open = default_inode_open,
                                             .unlink = ramfs_unlink,
                                             .set_symlink = ramfs_mksymlink,
                                             .read_symlink = ramfs_read_symlink,
                                             .rename = default_inode_rename };

const struct v_file_ops ramfs_file_ops = { .readdir = ramfs_readdir,
                                           .close = default_file_close,
                                           .read = default_file_read,
                                           .read_page = default_file_read_page,
                                           .write = default_file_write,
                                           .write_page =
                                             default_file_write_page,
                                           .seek = default_file_seek };