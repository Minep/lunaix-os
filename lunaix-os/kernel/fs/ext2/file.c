#include <lunaix/mm/valloc.h>
#include <lunaix/mm/page.h>
#include "ext2.h"

#define blkpos(e_sb, fpos) (fpos / e_sb->block_size)
#define blkoff(e_sb, fpos) (fpos / e_sb->block_size)

int
ext2_open_inode(struct v_inode* inode, struct v_file* file)
{
    int errno = 0;
    struct ext2_file* e_file;

    e_file = valloc(sizeof(*e_file));
    e_file->b_ino = EXT2_INO(inode);
    
    file->data = e_file;

    if (check_directory_node(inode)) {
        errno = ext2dr_open(inode, file);
        goto done;
    }

    // TODO anything for regular file?

done:
    if (!errno) {
        return 0;
    }

    vfree(e_file);
    file->data = NULL;
    return errno;
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
    if (check_directory_node(file->inode)) {
        return ext2dr_seek(file, offset);
    }

    //TODO
    return 0;
}

int
ext2_inode_read(struct v_inode *inode, void *buffer, size_t len, size_t fpos)
{
    struct ext2_sbinfo* e_sb;
    struct ext2_iterator iter;
    struct ext2b_inode* b_ino;
    unsigned int off;
    unsigned int end;
    unsigned int sz = 0, blksz, movsz;
    
    e_sb = EXT2_SB(inode->sb);
    b_ino = EXT2_INO(inode)->ino;
    blksz = e_sb->block_size;
    end = fpos + len;

    ext2db_itbegin(&iter, inode);
    ext2db_itffw(&iter, fpos / blksz);

    while (fpos < end && ext2db_itnext(&iter)) {
        off = fpos % blksz;
        movsz = MIN(end - fpos, blksz - off);
        
        memcpy(buffer, offset(iter.data, off), movsz);

        buffer = offset(buffer, movsz);
        fpos += movsz;
        sz += movsz;
    }

    ext2db_itend(&iter);
    return itstate_sel(&iter, MIN(sz, b_ino->i_size));
}

int
ext2_inode_read_page(struct v_inode *inode, void *buffer, size_t fpos)
{
    struct ext2_sbinfo* e_sb;
    struct ext2_iterator iter;
    struct ext2b_inode* b_ino;
    unsigned int blk_start, n, 
                 transfer_sz, total_sz = 0;

    assert(!va_offset(fpos));

    e_sb = EXT2_SB(inode->sb);
    b_ino = EXT2_INO(inode)->ino;
    
    blk_start = fpos / e_sb->block_size;
    n = PAGE_SIZE / e_sb->block_size;
    transfer_sz = MIN(PAGE_SIZE, e_sb->block_size);

    ext2db_itbegin(&iter, inode);
    ext2db_itffw(&iter, blk_start);

    while (n-- && ext2db_itnext(&iter)) 
    {
        memcpy(buffer, iter.data, transfer_sz);
        buffer = offset(buffer, transfer_sz);
        total_sz += transfer_sz;
    }
    
    ext2db_itend(&iter);
    return itstate_sel(&iter, MIN(total_sz, b_ino->i_size));
}

int
ext2_inode_write(struct v_inode *inode, void *buffer, size_t len, size_t fpos)
{
    int errno;
    unsigned int acc, blk_off, end, size;
    struct ext2_sbinfo* e_sb;
    bbuf_t buf;

    e_sb  = EXT2_SB(inode->sb);

    acc = 0;
    end = fpos + len;
    while (fpos < end) {
        errno = ext2db_acquire(inode, blkpos(e_sb, fpos), &buf);
        if (errno) {
            return errno;
        }

        blk_off = blkoff(e_sb, fpos);
        size = e_sb->block_size - blk_off;

        memcpy(offset(blkbuf_data(buf), blk_off), buffer, size);
        buffer = offset(buffer, size);

        fpos += blk_off;
        acc += size;
    }

    // TODO update metadata

    return (int)acc;
}

int
ext2_inode_write_page(struct v_inode *inode, void *buffer, size_t fpos)
{
    return ext2_inode_write(inode, buffer, PAGE_SIZE, fpos);
}

int
ext2_get_symlink(struct v_inode *this, const char **path_out)
{
    // TODO
    return 0;
}

int
ext2_set_symlink(struct v_inode *this, const char *target)
{
    // TODO
    return 0;
}