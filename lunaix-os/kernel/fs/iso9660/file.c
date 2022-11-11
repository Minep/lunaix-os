#include <lunaix/fs.h>
#include <lunaix/fs/iso9660.h>

int
iso9660_open(struct v_inode* this, struct v_file* file)
{
    // TODO
    return 0;
}

int
iso9660_close(struct v_file* file)
{
    // TODO
    return 0;
}

int
iso9660_read(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    // TODO
    return 0;
}

int
iso9660_write(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    // TODO
    return EROFS;
}

int
iso9660_seek(struct v_inode* inode, size_t offset)
{
    // TODO
    return 0;
}