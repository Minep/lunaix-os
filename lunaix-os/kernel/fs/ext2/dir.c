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
    struct ext2_iterator iter;
    struct ext2b_dirent *dir, *prev = NULL;
    bbuf_t prev_buf = NULL;

    ext2dr_itbegin(&iter, inode);

    while (ext2dr_itnext(&iter)) {
        dir = iter.dirent;

        if (dir->name_len != name->len) {
            continue;
        }

        if (strneq(dir->name, name->value, name->len)) {
            goto done;
        }

        prev = dir;
        
        if (prev_buf) {
            fsblock_put(prev_buf);
        }
        prev_buf = fsblock_take(iter.sel_buf);
    }

    ext2dr_itend(&iter);
    return itstate_sel(&iter, ENOENT);

done:
    e_dnode_out->self = (struct ext2_dnode_sub) {
        .buf = fsblock_take(iter.sel_buf),
        .dirent = dir
    };

    e_dnode_out->prev = (struct ext2_dnode_sub) {
        .buf = fsblock_take(prev_buf),
        .dirent = prev
    };

    ext2dr_itend(&iter);
    return 0;
}

static size_t
__dirent_realsize(struct ext2b_dirent* dirent)
{
    return sizeof(*dirent) - sizeof(dirent->name) + dirent->name_len;
}

static int
__find_free_dirent_slot(struct v_inode* inode, size_t size, 
                        struct ext2_dnode* e_dnode_out, size_t *reclen)
{
    struct ext2_iterator iter;
    struct ext2b_dirent *dir = NULL;
    bbuf_t prev_buf = NULL;
    bool found = false;

    ext2db_itbegin(&iter, inode);

    size_t sz = 0;
    unsigned int rec = 0, total_rec = 0;

    while (!found && ext2db_itnext(&iter))
    {
        rec = 0;
        do {
            dir = (struct ext2b_dirent*)offset(iter.data, rec);

            sz = dir->rec_len - __dirent_realsize(dir);
            sz = ROUNDDOWN(sz, 4);
            if (sz >= size) {
                found = true;
                break;
            }

            rec += dir->rec_len;
            total_rec += dir->rec_len;
        } while(rec < iter.blksz);

        if (likely(prev_buf)) {
            fsblock_put(prev_buf);
        }
        
        prev_buf = fsblock_take(iter.sel_buf);
    }

    assert_fs(dir);

    e_dnode_out->prev = (struct ext2_dnode_sub) {
        .buf = fsblock_take(prev_buf),
        .dirent = dir
    };

    if (!found) {
        // if prev is the last, and no more space left behind.
        assert_fs(rec == iter.blksz);
        
        e_dnode_out->self.buf = bbuf_null;
        ext2db_itend(&iter);
        return itstate_sel(&iter, 1);
    }

    unsigned int dir_size;

    dir_size = ROUNDUP(__dirent_realsize(dir), 4);
    *reclen = dir_size;

    rec = total_rec + dir_size;
    dir = (struct ext2b_dirent*)offset(iter.data, rec);
    
    e_dnode_out->self = (struct ext2_dnode_sub) {
        .buf = fsblock_take(iter.sel_buf),
        .dirent = dir
    };

    ext2db_itend(&iter);
    return 0;
}

static inline void
__destruct_ext2_dnode(struct ext2_dnode* e_dno)
{
    fsblock_put(e_dno->prev.buf);
    fsblock_put(e_dno->self.buf);
    vfree(e_dno);
}

static void
ext2dr_dnode_destruct(struct v_dnode* dnode)
{
    struct ext2_dnode* e_dno;

    e_dno = EXT2_DNO(dnode);
    __destruct_ext2_dnode(e_dno);
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

#define FT_REG  1
#define FT_DIR  2
#define FT_CHR  3
#define FT_BLK  4
#define FT_SYM  7

static unsigned int
__dir_filetype(struct ext2b_dirent* dir)
{
    unsigned int type;

    type = dir->file_type;

    if (type == FT_REG) {
        return VFS_IFFILE;
    }
    
    if (type == FT_DIR) {
        return VFS_IFDIR;
    }
    
    if (type == FT_BLK) {
        return VFS_IFVOLDEV;
    }
    
    if (type == FT_CHR) {
        return VFS_IFSEQDEV;
    }
    
    if (type == FT_SYM) {
        return VFS_IFSYMLINK;
    }

    return VFS_IFDEV;
}

int
ext2dr_read(struct v_file *file, struct dir_context *dctx)
{
    struct ext2_file* e_file;
    struct ext2b_dirent* dir;
    struct ext2_iterator* iter;

    e_file = EXT2_FILE(file);
    iter   = &e_file->iter;

    if (!ext2dr_itnext(&e_file->iter)) {
        return itstate_sel(iter, 0);
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

    return itstate_sel(iter, 0);
}

int
ext2dr_insert(struct v_inode* this, struct ext2b_dirent* dirent,
              struct ext2_dnode** e_dno_out)
{
    int errno;
    size_t size, new_reclen, old_reclen;
    struct ext2_inode* e_self;
    struct ext2_dnode*  e_dno;
    struct ext2b_dirent* prev_dirent;

    e_self = EXT2_INO(this);
    e_dno  = vzalloc(sizeof(*e_dno));
    
    size = __dirent_realsize(dirent);
    errno = __find_free_dirent_slot(this, size, e_dno, &new_reclen);
    if (errno < 0) {
        goto failed;
    }

    prev_dirent = e_dno->prev.dirent;
    old_reclen = prev_dirent->rec_len;

    if (errno > 0) {
        // prev is last record
        bbuf_t buf;
        if ((errno = ext2db_alloc(this, &buf))) {
            goto failed;
        }

        new_reclen = __dirent_realsize(prev_dirent);
        new_reclen = ROUNDUP(new_reclen, sizeof(int));
        e_dno->self = (struct ext2_dnode_sub) {
            .buf = buf,
            .dirent = block_buffer(buf, struct ext2b_dirent)
        };
    }

    /*
                   --- +--------+
                    ^  |  prev  |
                    |  +--------+
                    |      ^
                    |      |  new_reclen
                    |      v
                    |  +--------+  \
                    |  | dirent |  size 
        old_reclen  |  +--------+  /
                    |      ^
                    |      |  dirent.reclen
                    v      v
                   --- +--------+
                       |  next  |
                       +--------+
    */

    old_reclen -= new_reclen;
    dirent->rec_len = ROUNDUP(old_reclen, sizeof(int));

    prev_dirent->rec_len = new_reclen;
    memcpy(e_dno->self.dirent, dirent, size);

    fsblock_dirty(e_dno->prev.buf);
    fsblock_dirty(e_dno->self.buf);

    if (!e_dno_out) {
        __destruct_ext2_dnode(e_dno);
    }
    else {
        *e_dno_out = e_dno;
    }

    return errno;

failed:
    __destruct_ext2_dnode(e_dno);
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

    return 0;
}

int
ext2_rmdir(struct v_inode* this, struct v_dnode* dnode)
{
    int errno;
    struct v_inode* self;
    struct ext2_inode* e_self;
    struct ext2_dnode* e_dno;

    self = dnode->inode;
    e_self = EXT2_INO(self);
    e_dno  = EXT2_DNO(dnode);

    if (__check_special(dnode)) {
        return EINVAL;
    }

    if (!__check_empty_dir(self)) {
        return ENOTEMPTY;
    }

    if ((errno = ext2ino_free(self->sb, e_self))) {
        return errno;
    }

    return ext2dr_remove(e_dno);
}

static int
__d_insert(struct v_inode* parent, struct v_inode* self,
           struct ext2b_dirent* dirent,
           struct hstr* name, struct ext2_dnode** e_dno_out)
{
    *dirent = (struct ext2b_dirent){
        .name_len = name->len    
    };
    strncpy(dirent->name, name->value, name->len);

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

    return 0;

cleanup:
    ext2ino_free(this->sb, e_created);
    __destruct_ext2_dnode(e_dno);

cleanup1:
    dnode->data = NULL;
    ext2ino_free(this->sb, e_created);
    vfs_i_free(i_created);

    return errno;
}

void
ext2dr_setup_dirent(struct ext2b_dirent* dirent, struct v_dnode* dnode)
{
    struct hstr* name = &dnode->name;
    *dirent = (struct ext2b_dirent){
        .name_len = name->len    
    };
    strncpy(dirent->name, name->value, name->len);
}

int
ext2_rename(struct v_inode* from_inode, struct v_dnode* from_dnode,
            struct v_dnode* to_dnode)
{
    int errno;

    errno = ext2_unlink(from_inode, from_dnode);
    if (errno) {
        return errno;
    }

    return ext2_link(from_inode, to_dnode);;
}