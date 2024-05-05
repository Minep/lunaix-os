#include <lunaix/mm/valloc.h>
#include <lunaix/mm/page.h>
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

#define offset(data, off)   \
            ((void*)&((unsigned char*)data)[off])

int
ext2_inode_read(struct v_inode *inode, void *buffer, size_t len, size_t fpos)
{
    struct ext2_sbinfo* e_sb;
    struct ext2_iterator iter;
    unsigned int off;
    unsigned int end;
    unsigned int sz, blksz, movsz;
    
    e_sb = EXT2_SB(inode->sb);
    blksz = e_sb->block_size;
    end = fpos + len;

    ext2db_itbegin(&iter, inode);
    ext2db_itffw(&iter, fpos / blksz);

    while (fpos < end && ext2db_itnext(&iter)) {
        off = fpos % blksz;
        movsz = MIN(end - fpos, blksz - off);
        
        memcpy(buffer, offset(iter.data, off), movsz);

        buffer = offset(buffer, movsz);
        fpos += blksz - off;
        sz += movsz;
    }

    ext2db_itend(&iter);
    return sz;
}

int
ext2_inode_read_page(struct v_inode *inode, void *buffer, size_t fpos)
{
    struct ext2_sbinfo* e_sb;
    struct ext2_iterator iter;
    unsigned int blk_start, n, 
                 transfer_sz, total_sz;

    assert(!va_offset(fpos));

    e_sb = EXT2_SB(inode->sb);
    
    blk_start = fpos / e_sb->block_size;
    n = PAGE_SIZE / e_sb->block_size + 1;
    transfer_sz = MIN(PAGE_SHIFT, e_sb->block_size);

    ext2db_itbegin(&iter, inode);
    ext2db_itffw(&iter, blk_start);

    while (n-- && ext2db_itnext(&iter)) 
    {
        memcpy(buffer, iter.data, transfer_sz);
        buffer = offset(buffer, transfer_sz);
        total_sz += transfer_sz;
    }
    
    ext2db_itend(&iter);
    return total_sz;
}

int
ext2_inode_write(struct v_inode *inode, void *buffer, size_t len, size_t fpos)
{
    // TODO
    return 0;
}

int
ext2_inode_write_page(struct v_inode *inode, void *buffer, size_t fpos)
{
    // TODO
    return 0;
}