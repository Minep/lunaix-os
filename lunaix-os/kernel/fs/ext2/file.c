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
    ext2ino_update(file->inode);
    vfree(file->data);
    return 0;
}

int
ext2_sync_inode(struct v_file* file)
{
    // TODO
    // a modification to an inode may involves multiple
    //  blkbuf scattering among different groups.
    // For now, we just sync everything, until we figure out
    //  a way to track each dirtied blkbuf w.r.t inode
    blkbuf_syncall(file->inode->sb->blks, false);

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

#define SYMLNK_INPLACE \
        sizeof(((struct ext2b_inode*)0)->i_block_arr)

static inline int
__readlink_symlink(struct v_inode *this, char* path)
{
    size_t size;
    char* link = NULL;
    int errno;
    bbuf_t buf;
    struct ext2_inode* e_ino;
    
    e_ino = EXT2_INO(this);
    size  = e_ino->ino->i_size;
    if (size <= SYMLNK_INPLACE) {
        link = (char*) e_ino->ino->i_block_arr;
        strncpy(path, link, size);
    }
    else {
        buf = ext2db_get(this, 0);
        if (blkbuf_errbuf(buf)) {
            return EIO;
        }

        link = blkbuf_data(buf);
        strncpy(path, link, size);

        fsblock_put(buf);
    }

    return 0;
}

int
ext2_get_symlink(struct v_inode *this, const char **path_out)
{
    int errno;
    size_t size;
    char* symlink;
    struct ext2_inode* e_ino;
    
    e_ino = EXT2_INO(this);
    size  = e_ino->ino->i_size;

    if (!size) {
        return ENOENT;
    }

    if (!e_ino->symlink) {
        symlink = valloc(size);
        if ((errno = __readlink_symlink(this, symlink))) {
            vfree(symlink);
            return errno;
        }

        e_ino->symlink = symlink;
    }

    *path_out = e_ino->symlink;

    return size;
}

int
ext2_set_symlink(struct v_inode *this, const char *target)
{
    int errno = 0;
    bbuf_t buf = NULL;
    char* link;
    size_t size, new_len;
    struct ext2_inode* e_ino;
    
    e_ino = EXT2_INO(this);
    size = e_ino->ino->i_size;
    new_len = strlen(target);

    if (new_len > this->sb->blksize) {
        return ENAMETOOLONG;
    }

    if (size != new_len) {
        vfree_safe(e_ino->symlink);
        e_ino->symlink = valloc(new_len);
    }
    
    link = (char*) e_ino->ino->i_block_arr;

    // if new size is shrinked to inplace range
    if (size > SYMLNK_INPLACE && new_len <= SYMLNK_INPLACE) {
        ext2db_free_pos(this, 0);
    }
    
    // if new size is too big to fit inpalce
    if (new_len > SYMLNK_INPLACE) {

        // repurpose the i_block array back to normal
        if (size <= SYMLNK_INPLACE) {
            memset(link, 0, SYMLNK_INPLACE);
        }

        errno = ext2db_acquire(this, 0, &buf);
        if (errno) {
            goto done;
        }

        link = blkbuf_data(buf);
    }

    strncpy(e_ino->symlink, target, new_len);
    strncpy(link, target, new_len);

    if (buf) {
        fsblock_put(buf);
    }

done:
    return errno;
}