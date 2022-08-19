#include <lunaix/fs.h>

int
default_file_close(struct v_file* file)
{
    return 0;
}

int
default_file_seek(struct v_inode* inode, size_t offset)
{
    return 0;
}

int
default_inode_open(struct v_inode* this, struct v_file* file)
{
    return 0;
}

int
default_file_read(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    return ENOTSUP;
}

int
default_file_write(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    return ENOTSUP;
}

int
default_file_readdir(struct v_file* file, struct dir_context* dctx)
{
    int i = 0;
    struct v_dnode *pos, *n;
    llist_for_each(pos, n, &file->dnode->children, siblings)
    {
        if (i < dctx->index) {
            i++;
            continue;
        }
        dctx->read_complete_callback(dctx, pos->name.value, pos->name.len, 0);
        break;
    }
}

int
default_inode_dirlookup(struct v_inode* this, struct v_dnode* dnode)
{
    return ENOENT;
}

int
default_inode_rename(struct v_inode* from_inode,
                     struct v_dnode* from_dnode,
                     struct v_dnode* to_dnode)
{
    return 0;
}

int
default_inode_sync(struct v_inode* this)
{
    return 0;
}

int
default_inode_rmdir(struct v_inode* this, struct v_dnode* dir)
{
    return ENOTSUP;
}

int
default_inode_mkdir(struct v_inode* this, struct v_dnode* dir)
{
    return ENOTSUP;
}

struct v_file_ops default_file_ops = { .close = default_file_close,
                                       .read = default_file_read,
                                       .seek = default_file_seek,
                                       .readdir = default_file_readdir };

struct v_inode_ops default_inode_ops = { .dir_lookup = default_inode_dirlookup,
                                         .sync = default_inode_sync,
                                         .open = default_inode_open,
                                         .rename = default_inode_rename,
                                         .rmdir = default_inode_rmdir };