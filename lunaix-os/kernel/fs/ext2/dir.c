#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <klibc/string.h>

#include "ext2.h"

static inline bool
aligned_reclen(struct ext2b_dirent* dirent)
{
    return !(dirent->rec_len % 4);
}

static int
__find_dirent_byname(struct v_inode* inode, struct hstr* name, 
                     struct ext2_dnode* e_dnode_out)
{
    int errno = 0;
    struct ext2_iterator iter;
    struct ext2b_dirent *dir = NULL, *prev = NULL;
    bbuf_t prev_buf = NULL;

    ext2dr_itbegin(&iter, inode);

    while (ext2dr_itnext(&iter)) {
        dir = iter.dirent;

        if (dir->name_len != name->len) {
            goto cont;
        }

        if (strneq(dir->name, name->value, name->len)) {
            goto done;
        }

cont:        
        prev = dir;
        if (prev_buf) {
            fsblock_put(prev_buf);
        }
        prev_buf = fsblock_take(iter.sel_buf);
    }
    
    errno = ENOENT;
    goto _ret;

done:
    e_dnode_out->self = (struct ext2_dnode_sub) {
        .buf = fsblock_take(iter.sel_buf),
        .dirent = dir
    };

    e_dnode_out->prev = (struct ext2_dnode_sub) {
        .buf = fsblock_take(prev_buf),
        .dirent = prev
    };
    
_ret:
    fsblock_put(prev_buf);
    ext2dr_itend(&iter);
    return itstate_sel(&iter, errno);
}

static size_t
__dirent_realsize(struct ext2b_dirent* dirent)
{
    return sizeof(*dirent) - sizeof(dirent->name) + dirent->name_len;
}

#define DIRENT_INSERT     0
#define DIRENT_APPEND     1

#define DIRENT_ALIGNMENT    sizeof(int)

struct dirent_locator
{
    size_t search_size;

    int state;
    struct ext2_dnode result;
    size_t new_prev_reclen;
    size_t db_pos;
};


static inline void must_inline
__init_locator(struct dirent_locator* loc, size_t search_size)
{
    *loc = (struct dirent_locator) { .search_size = search_size };
}

static int
__find_free_dirent_slot(struct v_inode* inode, struct dirent_locator* loc)
{
    struct ext2_iterator dbit;
    struct ext2b_dirent *dir = NULL;
    struct ext2_dnode* result;
    
    bbuf_t prev_buf = bbuf_null;
    bool found = false;

    size_t sz = 0, aligned = 0;
    unsigned int rec = 0, total_rec = 0;
    unsigned int dir_size;

    aligned = ROUNDUP(loc->search_size, DIRENT_ALIGNMENT);
    result  = &loc->result;

    ext2db_itbegin(&dbit, inode, DBIT_MODE_BLOCK);

    while (!found && ext2db_itnext(&dbit))
    {
        rec = 0;
        do {
            dir = (struct ext2b_dirent*)offset(dbit.data, rec);

            sz = dir->rec_len - __dirent_realsize(dir);
            sz = ROUNDDOWN(sz, DIRENT_ALIGNMENT);
            if ((signed)sz >= (signed)aligned) {
                found = true;
                break;
            }

            rec += dir->rec_len;
            total_rec += dir->rec_len;
        } while(rec < dbit.blksz);

        if (likely(prev_buf)) {
            fsblock_put(prev_buf);
        }
        
        prev_buf = fsblock_take(dbit.sel_buf);
    }

    ext2_debug("dr_find_slot: found=%d, blk_off=%d, off=%d, gap=%d, blk=%d/%d", 
                found, rec, total_rec, sz, dbit.pos - 1, dbit.end_pos);

    loc->db_pos = dbit.pos - 1;

    if (blkbuf_nullbuf(prev_buf)) {
        // this dir is brand new
        loc->state = DIRENT_APPEND;
        goto done;
    }

    dir_size = ROUNDUP(__dirent_realsize(dir), 4);
    loc->new_prev_reclen = dir_size;

    result->prev = (struct ext2_dnode_sub) {
        .buf = fsblock_take(prev_buf),
        .dirent = dir
    };

    if (!found) {
        // if prev is the last, and no more space left behind.
        assert_fs(rec == dbit.blksz);
        
        result->self.buf = bbuf_null;
        ext2db_itend(&dbit);

        loc->state = DIRENT_APPEND;
        goto done;
    }

    rec += dir_size;
    dir = (struct ext2b_dirent*)offset(dbit.data, rec);
    
    result->self = (struct ext2_dnode_sub) {
        .buf = fsblock_take(dbit.sel_buf),
        .dirent = dir
    };

    ext2db_itend(&dbit);

    loc->state = DIRENT_INSERT;

done:
    return itstate_sel(&dbit, 0);
}

static inline void
__release_dnode_blocks(struct ext2_dnode* e_dno)
{
    fsblock_put(e_dno->prev.buf);
    fsblock_put(e_dno->self.buf);
}

static inline void
__destruct_ext2_dnode(struct ext2_dnode* e_dno)
{
    __release_dnode_blocks(e_dno);
    vfree(e_dno);
}

static inline bool
__check_special(struct v_dnode* dnode)
{
    return HSTR_EQ(&dnode->name, &vfs_dot)
        || HSTR_EQ(&dnode->name, &vfs_ddot);
}

static bool
__check_empty_dir(struct v_inode* dir_ino)
{
    struct ext2_iterator iter;
    struct ext2b_dirent* dir;
    
    ext2dr_itbegin(&iter, dir_ino);
    while (ext2dr_itnext(&iter))
    {
        dir = iter.dirent;
        if (strneq(dir->name, vfs_dot.value, 1)) {
            continue;
        }

        if (strneq(dir->name, vfs_ddot.value, 2)) {
            continue;
        }

        ext2dr_itend(&iter);
        return false;
    }

    ext2dr_itend(&iter);
    return true;
}

void
ext2dr_itbegin(struct ext2_iterator* iter, struct v_inode* inode)
{
    *iter = (struct ext2_iterator){
        .pos = 0,
        .inode = inode,
        .blksz = inode->sb->blksize
    };

    iter->sel_buf = ext2db_get(inode, 0);
    ext2_itcheckbuf(iter);
}

void
ext2dr_itreset(struct ext2_iterator* iter)
{
    fsblock_put(iter->sel_buf);
    iter->sel_buf = ext2db_get(iter->inode, 0);
    ext2_itcheckbuf(iter);

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
        fsblock_put(iter->sel_buf);
    }
}

bool
ext2dr_itnext(struct ext2_iterator* iter)
{
    struct ext2b_dirent* d;
    unsigned int blkpos, db_index;
    bbuf_t buf;
    
    buf = iter->sel_buf;

    if (iter->has_error) {
        return false;
    }

    if (likely(iter->dirent)) {
        d = iter->dirent;
        
        assert_fs(!(d->rec_len % 4));
        iter->pos += d->rec_len;

        if (!d->rec_len || !d->inode) {
            return false;
        }
    }

    blkpos = iter->pos % iter->blksz;
    db_index = iter->pos / iter->blksz;
    
    if (unlikely(iter->pos >= iter->blksz)) {
        fsblock_put(buf);

        buf = ext2db_get(iter->inode, db_index);
        iter->sel_buf = buf;

        if (!buf || !ext2_itcheckbuf(iter)) {
            return false;
        }
    }

    d = (struct ext2b_dirent*)offset(blkbuf_data(buf), blkpos);
    iter->dirent = d;

    return true;
}

int
ext2dr_open(struct v_inode* this, struct v_file* file)
{
    struct ext2_file* e_file;

    e_file = EXT2_FILE(file);

    ext2dr_itbegin(&e_file->iter, this);

    return itstate_sel(&e_file->iter, 0);
}

int
ext2dr_close(struct v_inode* this, struct v_file* file)
{
    struct ext2_file* e_file;

    e_file = EXT2_FILE(file);

    ext2dr_itend(&e_file->iter);

    return 0;
}

int
ext2dr_lookup(struct v_inode* inode, struct v_dnode* dnode)
{
    int errno;
    struct ext2b_dirent* dir;
    struct ext2_dnode* e_dnode;
    struct v_inode* dir_inode;

    e_dnode = valloc(sizeof(struct ext2_dnode));
    errno = __find_dirent_byname(inode, &dnode->name, e_dnode);
    if (errno) {
        vfree(e_dnode);
        return errno;
    }

    dir = e_dnode->self.dirent;
    if (!(dir_inode = vfs_i_find(inode->sb, dir->inode))) { 
        dir_inode = vfs_i_alloc(inode->sb);
        ext2ino_fill(dir_inode, dir->inode);
    }

    dnode->data = e_dnode;
    vfs_assign_inode(dnode, dir_inode);

    return 0;
}

#define FT_NUL  0
#define FT_REG  1
#define FT_DIR  2
#define FT_CHR  3
#define FT_BLK  4
#define FT_SYM  7
#define check_imode(val, imode)     (((val) & (imode)) == (imode)) 

static inline unsigned int
__imode_to_filetype(unsigned int imode)
{
    if (check_imode(imode, IMODE_IFLNK)) {
        return FT_SYM;
    }

    if (check_imode(imode, IMODE_IFBLK)) {
        return FT_BLK;
    }

    if (check_imode(imode, IMODE_IFCHR)) {
        return FT_CHR;
    }

    if (check_imode(imode, IMODE_IFDIR)) {
        return FT_DIR;
    }

    if (check_imode(imode, IMODE_IFREG)) {
        return FT_REG;
    }

    return FT_NUL;
}

static int
__dir_filetype(struct v_superblock* vsb, struct ext2b_dirent* dir)
{
    int errno;
    unsigned int type;

    if (ext2_feature(vsb, FEAT_FILETYPE)) {
        type = dir->file_type;
    }
    else {
        struct ext2_fast_inode e_fino;

        errno = ext2ino_get_fast(vsb, dir->inode, &e_fino);
        if (errno) {
            return errno;
        }

        type = __imode_to_filetype(e_fino.ino->i_mode);

        fsblock_put(e_fino.buf);
    }
    
    if (type == FT_DIR) {
        return DT_DIR;
    }
    
    if (type == FT_SYM) {
        return DT_SYMLINK;
    }

    return DT_FILE;
}

int
ext2dr_read(struct v_file *file, struct dir_context *dctx)
{
    struct ext2_file* e_file;
    struct ext2b_dirent* dir;
    struct ext2_iterator* iter;
    struct v_superblock* vsb;
    int dirtype;

    e_file = EXT2_FILE(file);
    vsb    = file->inode->sb;
    iter   = &e_file->iter;

    if (!ext2dr_itnext(&e_file->iter)) {
        return itstate_sel(iter, 0);
    }

    dir = e_file->iter.dirent;
    dirtype =  __dir_filetype(vsb, dir);
    if (dirtype < 0) {
        return dirtype;
    }

    fsapi_dir_report(dctx, dir->name, dir->name_len, dirtype);
    
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

    return itstate_sel(iter, 0);
}

int
ext2dr_insert(struct v_inode* this, struct ext2b_dirent* dirent,
              struct ext2_dnode** e_dno_out)
{
    int errno;
    size_t size, new_reclen, old_reclen;
    struct ext2_dnode*  e_dno;
    struct ext2b_dirent* prev_dirent;
    struct dirent_locator locator;
    bbuf_t buf;

    size = __dirent_realsize(dirent);
    __init_locator(&locator, size);
    
    errno = __find_free_dirent_slot(this, &locator);
    if (errno < 0) {
        goto failed;
    }

    e_dno = &locator.result;
    new_reclen = locator.new_prev_reclen;
    old_reclen = fsapi_block_size(this->sb);

    if (locator.state != DIRENT_INSERT) 
    {
        if ((errno = ext2db_acquire(this, locator.db_pos, &buf)))
            goto failed;

        this->fsize += fsapi_block_size(this->sb);
        ext2ino_update(this);
        
        e_dno->self.buf = buf;
        e_dno->self.dirent = block_buffer(buf, struct ext2b_dirent);
    }


    /*
                   --- +--------+ ---
                    ^  |  prev  |  |
                    |  +--------+  |
                    |              |   new_reclen
                    |              |
                    |              v
                    |  +--------+ ---  -
                    |  | dirent |  |   | size
        old_reclen  |  +--------+  |   - 
                    |              |   dirent.reclen
                    |              |
                    v              v
                   --- +--------+ ---
                       |  next  |
                       +--------+
    */

    else
    {
        prev_dirent = e_dno->prev.dirent;
        old_reclen  = prev_dirent->rec_len;
        old_reclen -= new_reclen;

        prev_dirent->rec_len = new_reclen;
        fsblock_dirty(e_dno->prev.buf);
    }

    ext2_debug("dr_insert: state=%d, blk=%d, prev_rlen=%d, new_rlen=%d", 
                locator.state, locator.db_pos, new_reclen, old_reclen);

    assert_fs(new_reclen > 0);
    assert_fs(old_reclen > 0);

    dirent->rec_len = old_reclen;
    
    memcpy(e_dno->self.dirent, dirent, size);
    fsblock_dirty(e_dno->self.buf);

    if (!e_dno_out) {
        __release_dnode_blocks(e_dno);
    }
    else {
        *e_dno_out = e_dno;
    }

    return errno;

failed:
    __release_dnode_blocks(e_dno);
    return errno;
}

int
ext2dr_remove(struct ext2_dnode* e_dno)
{
    struct ext2_dnode_sub *dir_prev, *dir;    
    assert(e_dno->prev.dirent);

    dir_prev = &e_dno->prev;
    dir = &e_dno->self;

    dir_prev->dirent->rec_len += dir->dirent->rec_len;
    dir->dirent->rec_len = 0;
    dir->dirent->inode = 0;

    fsblock_dirty(dir_prev->buf);
    fsblock_dirty(dir->buf);

    __destruct_ext2_dnode(e_dno);

    return 0;
}

int
ext2_rmdir(struct v_inode* this, struct v_dnode* dnode)
{
    int errno;
    struct v_inode* self;
    struct ext2_dnode* e_dno;

    self = dnode->inode;
    e_dno  = EXT2_DNO(dnode);

    if (__check_special(dnode)) {
        return EINVAL;
    }

    if (!__check_empty_dir(self)) {
        return ENOTEMPTY;
    }

    if ((errno = ext2ino_free(self))) {
        return errno;
    }

    return ext2dr_remove(e_dno);
}

static int
__d_insert(struct v_inode* parent, struct v_inode* self,
           struct ext2b_dirent* dirent,
           struct hstr* name, struct ext2_dnode** e_dno_out)
{
    ext2dr_setup_dirent(dirent, self, name);
    
    dirent->inode = self->id;
    return ext2dr_insert(parent, dirent, e_dno_out);
}

int
ext2_mkdir(struct v_inode* this, struct v_dnode* dnode)
{
    int errno;
    struct ext2_inode *e_contain, *e_created;
    struct v_inode* i_created;
    struct ext2_dnode* e_dno = NULL;
    struct ext2b_dirent dirent;

    e_contain = EXT2_INO(this);

    errno = ext2ino_make(this->sb, VFS_IFDIR, e_contain, &i_created);
    if (errno) {
        return errno;
    }

    e_created = EXT2_INO(i_created);

    if ((errno = __d_insert(this, i_created, &dirent, &dnode->name, &e_dno))) {
        goto cleanup1;
    }

    // link the created dir inode to dirent
    ext2ino_linkto(e_created, &dirent);
    dnode->data = e_dno;

    // insert . and ..
    // we don't need ext2ino_linkto here.

    if ((errno = __d_insert(i_created, i_created, &dirent, &vfs_dot, NULL))) {
        goto cleanup;
    }

    if ((errno = __d_insert(i_created, this, &dirent, &vfs_ddot, NULL))) {
        goto cleanup;
    }

    vfs_assign_inode(dnode, i_created);
    return 0;

cleanup:
    __destruct_ext2_dnode(e_dno);

cleanup1:
    dnode->data = NULL;
    ext2ino_free(i_created);
    vfs_i_free(i_created);

    return errno;
}

void
ext2dr_setup_dirent(struct ext2b_dirent* dirent, 
                    struct v_inode* inode, struct hstr* name)
{
    unsigned int imode;
    
    imode = EXT2_INO(inode)->ino->i_mode;
    *dirent = (struct ext2b_dirent){
        .name_len = name->len
    };
    
    strncpy(dirent->name, name->value, name->len);

    if (ext2_feature(inode->sb, FEAT_FILETYPE)) {
        dirent->file_type = __imode_to_filetype(imode);
    }
}

int
ext2_rename(struct v_inode* from_inode, struct v_dnode* from_dnode,
            struct v_dnode* to_dnode)
{
    int errno;
    struct v_inode* to_parent;

    if (EXT2_DNO(to_dnode)) {
        errno = ext2_unlink(to_dnode->inode, to_dnode);
        if (errno) {
            return errno;
        }
    }

    errno = ext2_link(from_inode, to_dnode);
    if (errno) {
        return errno;
    }

    return ext2_unlink(from_inode, from_dnode);
}