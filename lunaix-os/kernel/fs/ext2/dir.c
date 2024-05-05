#include "ext2.h"

#include <klibc/string.h>

void
ext2dr_itbegin(struct ext2_iterator* iter, struct v_inode* inode)
{
    *iter = (struct ext2_iterator){
        .pos = 0,
        .blk_pos = 0,
        .inode = inode,
        .block_end = inode->sb->blksize
    };

    iter->sel_buf = ext2_get_data_block(inode, 0);
}

void
ext2dr_itreset(struct ext2_iterator* iter)
{
    fsblock_put(iter->inode->sb, iter->sel_buf);
    iter->sel_buf = ext2_get_data_block(iter->inode, 0);

    iter->blk_pos = 0;
    iter->pos = 0;
}

int
ext2dr_itffw(struct ext2_iterator* iter, int count)
{
    int i = 0;
    while (i < count && ext2dr_itnext(iter)) {
        i++;
    }

    return i;
}

void
ext2dr_itend(struct ext2_iterator* iter)
{
    if (iter->sel_buf) {
        fsblock_put(iter->inode->sb, iter->sel_buf);
    }
}

bool
ext2dr_itnext(struct ext2_iterator* iter)
{
    struct ext2b_dirent* d;
    bbuf_t buf;
    
    buf = iter->sel_buf;

    if (likely(iter->dirent)) {
        d = iter->dirent;
        iter->pos += d->rec_len;

        if (!d->inode) {
            return false;
        }
    }
    
    if (unlikely(iter->pos >= iter->block_end)) {
        iter->blk_pos++;
        iter->pos %= iter->block_end;

        fsblock_put(iter->inode->sb, buf);

        buf = ext2_get_data_block(iter->inode, iter->blk_pos);
        iter->sel_buf = buf;

        if (!buf) {
            return false;
        }
    }

    d = &block_buffer(buf, struct ext2b_dirent)[iter->pos];
    iter->dirent = d;

    return true;
}

int
ext2dr_open(struct v_inode* this, struct v_file* file)
{
    struct ext2_file* e_file;

    e_file = EXT2_FILE(file);

    ext2dr_itbegin(&e_file->iter, this);

    return 0;
}

int
ext2dr_lookup(struct v_inode* inode, struct v_dnode* dnode)
{
    struct ext2_iterator iter;
    struct ext2b_dirent* dir;
    struct hstr* name = &dnode->name;

    ext2dr_itbegin(&iter, inode);

    while (ext2dr_itnext(&iter)) {
        dir = iter.dirent;

        if (dir->name_len != name->len) {
            continue;
        }

        if (streq(dir->name, name->value)) {
            goto done;
        }
    }

    ext2dr_itend(&iter);
    return ENOENT;

done:
    struct v_inode* dir_inode;

    if (!(dir_inode = vfs_i_find(inode->sb, dir->inode))) { 
        dir_inode = vfs_i_alloc(inode->sb);
        ext2_fill_inode(dir_inode, dir->inode);
    }

    vfs_assign_inode(dnode, dir_inode);

    ext2dr_itend(&iter);

    return 0;
}

#define FT_REG  1
#define FT_DIR  2
#define FT_CHR  3
#define FT_BLK  4
#define FT_SYM  7

static inline unsigned int
__dir_filetype(struct ext2b_dirent* dir)
{
    if (dir->file_type == FT_REG) {
        return VFS_IFFILE;
    }
    else if (dir->file_type == FT_DIR) {
        return VFS_IFDIR;
    }
    else if (dir->file_type == FT_BLK) {
        return VFS_IFVOLDEV;
    }
    else if (dir->file_type == FT_CHR) {
        return VFS_IFSEQDEV;
    }
    else if (dir->file_type == FT_SYM) {
        return VFS_IFSYMLINK;
    }

    return VFS_IFDEV;
}

int
ext2dr_read(struct v_file *file, struct dir_context *dctx)
{
    struct ext2_file* e_file;
    struct ext2b_dirent* dir;

    e_file = EXT2_FILE(file);

    if (!ext2dr_itnext(&e_file->iter)) {
        return 0;
    }

    dir = e_file->iter.dirent;
    fsapi_dir_report(dctx, dir->name, dir->name_len, __dir_filetype(dir));
    
    return 1;
}

int
ext2dr_seek(struct v_file* file, size_t offset)
{
    struct ext2_file* e_file;
    struct ext2_iterator* iter;
    unsigned int fpos;

    e_file = EXT2_FILE(file);
    iter = &e_file->iter;
    fpos = file->f_pos;

    if (offset == fpos) {
        return 0;
    }

    if (offset > fpos) {
        fpos = ext2dr_itffw(iter, fpos - offset);
        return 0;
    }

    if (!offset || offset < fpos) {
        ext2dr_itreset(iter);
    }

    fpos = ext2dr_itffw(iter, offset);

    return 0;
}