#include <lunaix/mm/valloc.h>
#include "ext2.h"

int
ext2_open_inode(struct v_inode* this, struct v_file* file)
{
    int errno;
    struct ext2_file* e_file;

    e_file = valloc(sizeof(*e_file));
    e_file->b_ino = EXT2_INO(this);

    if (this->itype == VFS_IFDIR) {
        errno = ext2dr_open(this, file);
        goto done;
    }

done:
    if (errno) {
        vfree(e_file);
        return errno;
    }

    file->data = e_file;
    return 0;
}

int
ext2_close_inode(struct v_file* file)
{
    vfree(file->data);
    return 0;
}

int
ext2_sync_inode(struct v_file* file)
{
    // TODO
    return 0;
}

int
ext2_seek_inode(struct v_file* file, size_t offset)
{
    if (file->inode->itype == VFS_IFDIR) {
        return ext2dr_seek(file, offset);
    }

    //TODO
    return 0;
}