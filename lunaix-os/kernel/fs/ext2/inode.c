#include <lunaix/fs/api.h>
#include <lunaix/mm/valloc.h>

#include <klibc/string.h>

#include "ext2.h"

#define IMODE_IFSOCK    0xc000
#define IMODE_IFLNK     0xA000
#define IMODE_IFREG     0x8000
#define IMODE_IFBLK     0x6000
#define IMODE_IFDIR     0x4000
#define IMODE_IFCHR     0x2000
#define IMODE_IFFIFO    0x1000

#define IMODE_URD       0x0100
#define IMODE_UWR       0x0080
#define IMODE_UEX       0x0040
#define IMODE_GRD       0x0020
#define IMODE_GWR       0x0010
#define IMODE_GEX       0x0008
#define IMODE_ORD       0x0004
#define IMODE_OWR       0x0002
#define IMODE_OEX       0x0001

struct walk_state
{
    unsigned int* slot_ref;
    bbuf_t table;
    int indirections;
    int level;
};

static struct v_inode_ops ext2_inode_ops = {
    .dir_lookup = ext2dr_lookup,
    .open  = ext2_open_inode,
    .mkdir = ext2_mkdir,
    .rmdir = ext2_rmdir,
    .read_symlink = ext2_get_symlink,
    .set_symlink  = ext2_set_symlink,
    .rename = ext2_rename,
    .link = ext2_link,
    .unlink = ext2_unlink,
    .create = ext2_create
};

static struct v_file_ops ext2_file_ops = {
    .close = ext2_close_inode,
    
    .read = ext2_inode_read,
    .read_page = ext2_inode_read_page,
    
    .write = ext2_inode_write,
    .write_page = ext2_inode_write_page,
    
    .readdir = ext2dr_read,
    .seek = ext2_seek_inode,
    .sync = ext2_sync_inode
};

#define to_tag(e_ino, val)        \
        (((val) >> (e_ino)->inds_lgents) | (1 << msbiti))
#define valid_tag(tag)      ((tag) & (1 << msbiti))

static void
__btlb_insert(struct v_inode* inode, unsigned int blkid, bbuf_t buf)
{
    struct ext2_inode* e_inode;
    struct ext2_btlb* btlb;
    struct ext2_btlb_entry* btlbe = NULL;
    unsigned int cap_sel;
 
    if (unlikely(!blkid)) {
        return;
    }

    e_inode = EXT2_INO(inode);
    btlb = e_inode->btlb;

    for (int i = 0; i < BTLB_SETS; i++)
    {
        if (valid_tag(btlb->buffer[i].tag)) {
            continue;
        }

        btlbe = &btlb->buffer[i];
        goto found;
    }

    /*
        we have triggered the capacity miss.
        since most file operation is heavily linear and strong
        locality, we place our bet on it and avoid go through
        the whole overhead of LRU eviction stuff. Just a trival
        random eviction will do the fine job
    */
    cap_sel = hash_32(blkid, ilog2(BTLB_SETS));
    btlbe = &btlb->buffer[cap_sel];

    fsblock_put(btlbe->block);

found:
    btlbe->tag = to_tag(e_inode, blkid);
    btlbe->block = fsblock_take(buf);
}

static bbuf_t
__btlb_hit(struct v_inode* inode, unsigned int blkid)
{
    struct ext2_inode* e_inode;
    struct ext2_btlb* btlb;
    struct ext2_btlb_entry* btlbe = NULL;
    unsigned int in_tag;

    e_inode = EXT2_INO(inode);
    btlb = e_inode->btlb;
    in_tag = to_tag(e_inode, blkid);

    for (int i = 0; i < BTLB_SETS; i++)
    {
        btlbe = &btlb->buffer[i];
        if (btlbe->tag == in_tag) {
            return fsblock_take(btlbe->block);
        }
    }

    return NULL;
}

static void
__btlb_flushall(struct v_inode* inode)
{
    struct ext2_inode* e_inode;
    struct ext2_btlb* btlb;
    struct ext2_btlb_entry* btlbe = NULL;

    e_inode = EXT2_INO(inode);
    btlb = e_inode->btlb;

    for (int i = 0; i < BTLB_SETS; i++)
    {
        btlbe = &btlb->buffer[i];
        if (!valid_tag(btlbe->tag)) {
            continue;
        }

        btlbe->tag = 0;
        fsblock_put(btlbe->block);
    }
}

void
ext2db_itbegin(struct ext2_iterator* iter, struct v_inode* inode)
{
    struct ext2b_inode* b_ino;

    b_ino = EXT2_INO(inode)->ino;
    *iter = (struct ext2_iterator){
        .pos = 0,
        .inode = inode,
        .blksz = inode->sb->blksize,
        .end_pos = ICEIL(b_ino->i_size, inode->sb->blksize)
    };
}

void
ext2db_itreset(struct ext2_iterator* iter)
{
    if (likely(iter->sel_buf)) {
        fsblock_put(iter->sel_buf);
        iter->sel_buf = NULL;
    }

    iter->pos = 0;
}

int
ext2db_itffw(struct ext2_iterator* iter, int count)
{
    iter->pos += count;
    return count;
}

void
ext2db_itend(struct ext2_iterator* iter)
{
    if (likely(iter->sel_buf)) {
        fsblock_put(iter->sel_buf);
        iter->sel_buf = NULL;
    }
}

bool
ext2db_itnext(struct ext2_iterator* iter)
{
    bbuf_t buf;

    if (unlikely(iter->has_error)) {
        return false;
    }

    if (unlikely(iter->pos > iter->end_pos)) {
        return false;
    }

    if (likely(iter->sel_buf)) {
        fsblock_put(iter->sel_buf);
    }

    buf = ext2db_get(iter->inode, iter->pos);
    iter->sel_buf = buf;

    if (!buf || !ext2_itcheckbuf(iter)) {
        return false;
    }

    iter->pos++;
    iter->data = blkbuf_data(buf);

    return true;
}

void
ext2ino_init(struct v_superblock* vsb, struct v_inode* inode)
{
    // Perhaps we dont need this at all...
}

void
ext2_destruct_inode(struct v_inode* inode)
{
    struct ext2_inode* e_inode;

    e_inode = EXT2_INO(inode);

    __btlb_flushall(inode);

    fsblock_put(e_inode->ind_ord1);
    fsblock_put(e_inode->buf);

    ext2gd_put(e_inode->blk_grp);

    vfree_safe(e_inode->symlink);
    vfree(e_inode->btlb);
    vfree(e_inode);
}

static inline void
__ext2ino_fill_common(struct v_inode* inode, ino_t ino_id)
{
    fsapi_inode_setid(inode, ino_id, ino_id);
    fsapi_inode_setfops(inode, &ext2_file_ops);
    fsapi_inode_setops(inode, &ext2_inode_ops);
    fsapi_inode_setdector(inode, ext2_destruct_inode);
}


static unsigned int
__translate_vfs_itype(unsigned int v_itype)
{
    unsigned int e_itype = IMODE_IFREG;

    if (v_itype == VFS_IFFILE) {
        e_itype = IMODE_IFREG;
    }
    else if (check_itype(v_itype, VFS_IFDIR)) {
        e_itype = IMODE_IFDIR;
    }
    else if (check_itype(v_itype, VFS_IFSEQDEV)) {
        e_itype = IMODE_IFCHR;
    }
    else if (check_itype(v_itype, VFS_IFVOLDEV)) {
        e_itype = IMODE_IFBLK;
    }
    
    if (check_itype(v_itype, VFS_IFSYMLINK)) {
        e_itype |= IMODE_IFLNK;
    }

    // FIXME we keep this until we have our own user manager
    e_itype |= (IMODE_URD | IMODE_GRD | IMODE_ORD);
    return e_itype;
}

int
ext2ino_fill(struct v_inode* inode, ino_t ino_id)
{
    struct ext2_sbinfo* sb;
    struct ext2_inode* e_ino;
    struct v_superblock* vsb;
    struct ext2b_inode* b_ino;
    unsigned int type = VFS_IFFILE;
    int errno = 0;

    vsb = inode->sb;
    sb = EXT2_SB(vsb);

    if ((errno = ext2ino_get(vsb, ino_id, &e_ino))) {
        return errno;
    }
    
    b_ino = e_ino->ino;
    ino_id = e_ino->ino_id;

    fsapi_inode_setsize(inode, b_ino->i_size);
    
    fsapi_inode_settime(inode, b_ino->i_ctime, 
                               b_ino->i_mtime, 
                               b_ino->i_atime);
    
    __ext2ino_fill_common(inode, ino_id);

    if (check_itype(b_ino->i_mode, IMODE_IFLNK)) {
        type = VFS_IFSYMLINK;
    }
    else if (check_itype(b_ino->i_mode, IMODE_IFDIR)) {
        type = VFS_IFDIR;
    }
    else if (check_itype(b_ino->i_mode, IMODE_IFCHR)) {
        type = VFS_IFSEQDEV;
    }
    else if (check_itype(b_ino->i_mode, IMODE_IFBLK)) {
        type = VFS_IFVOLDEV;
    }

    fsapi_inode_settype(inode, type);

    fsapi_inode_complete(inode, e_ino);

    return 0;
}

static struct ext2_inode*
__create_inode(struct v_superblock* vsb, struct ext2_gdesc* gd, int ino_index)
{
    bbuf_t ino_tab;
    struct ext2_sbinfo* sb;
    struct ext2b_inode* b_inode;
    struct ext2_inode* inode;
    unsigned int ino_tab_sel, ino_tab_off, 
                 tab_partlen, ind_ents, inds_blks;

    sb = gd->sb;
    tab_partlen = sb->block_size / sb->raw->s_ino_size;
    ino_tab_sel = ino_index / tab_partlen;
    ino_tab_off = ino_index % tab_partlen;

    ino_tab = fsblock_get(vsb, gd->info->bg_ino_tab + ino_tab_sel);
    if (blkbuf_errbuf(ino_tab)) {
        return NULL;
    }

    b_inode = (struct ext2b_inode*)blkbuf_data(ino_tab);
    
    inode            = vzalloc(sizeof(*inode));
    inode->btlb      = vzalloc(sizeof(struct ext2_btlb));
    inode->buf       = ino_tab;
    inode->ino       = &b_inode[ino_tab_off];
    inode->blk_grp   = gd;

    if (b_inode->i_blocks) {
        inds_blks = b_inode->i_blocks - ICEIL(b_inode->i_size, 512);
        inds_blks = inds_blks / (sb->block_size / 512);
        inode->indirect_blocks = inds_blks;
    }

    ind_ents = sb->block_size / sizeof(int);
    assert(is_pot(ind_ents));

    inode->inds_lgents = ilog2(ind_ents);
    inode->ino_id = gd->ino_base + to_ext2ino_id(ino_index);

    return inode;
}

int
ext2ino_get(struct v_superblock* vsb, 
               unsigned int ino, struct ext2_inode** out)
{
    struct ext2_sbinfo* sb;
    struct ext2_inode* inode;
    struct ext2_gdesc* gd;
    struct ext2b_inode* b_inode;
    unsigned int blkgrp_id, ino_rel_id;
    unsigned int tab_partlen;
    unsigned int ind_ents, prima_ind;
    int errno = 0;
    
    sb = EXT2_SB(vsb);

    blkgrp_id   = to_fsblock_id(ino) / sb->raw->s_ino_per_grp;
    ino_rel_id  = to_fsblock_id(ino) % sb->raw->s_ino_per_grp;

    if ((errno = ext2gd_take(vsb, blkgrp_id, &gd))) {
        return errno;
    }

    inode = __create_inode(vsb, gd, ino_rel_id);
    if (!inode) {
        return EIO;
    }
    
    b_inode = inode->ino;
    prima_ind = b_inode->i_block.ind1;
    *out = inode;

    if (prima_ind) {
        return errno;
    }

    inode->ind_ord1 = fsblock_get(vsb, prima_ind);
    if (blkbuf_errbuf(inode->ind_ord1)) {
        vfree(inode->btlb);
        vfree(inode);
        *out = NULL;
        return EIO;
    }

    return errno;
}

int
ext2ino_alloc(struct v_superblock* vsb, 
                 struct ext2_inode* hint, struct ext2_inode** out)
{
    int free_ino_idx;
    struct ext2_gdesc* gd;
    struct ext2_inode* inode;

    free_ino_idx = ALLOC_FAIL;
    if (hint) {
        gd = hint->blk_grp;
        free_ino_idx = ext2gd_alloc_inode(gd);
    }

    // locality hinted alloc failed, try entire fs
    if (!valid_bmp_slot(free_ino_idx)) {
        free_ino_idx = ext2ino_alloc_slot(vsb, &gd);
    }

    if (!valid_bmp_slot(free_ino_idx)) {
        return EDQUOT;
    }

    inode = __create_inode(vsb, gd, free_ino_idx);
    if (!inode) {
        // what a shame!
        ext2gd_free_inode(gd, free_ino_idx);
        return EIO;
    }

    memset(inode->ino, 0, sizeof(*inode->ino));
    fsblock_dirty(inode->buf);

    *out = inode;
    return 0;
}

static inline int
__free_block_at(struct v_superblock *vsb, unsigned int block_pos)
{
    int errno, gd_index;
    struct ext2_gdesc* gd;
    struct ext2_sbinfo * sb;

    if (!block_pos) {
        return 0;
    }

    sb = EXT2_SB(vsb);
    gd_index = block_pos / sb->raw->s_blk_per_grp;

    if ((errno = ext2gd_take(vsb, gd_index, &gd))) {
        return errno;
    }

    assert(block_pos >= gd->base);
    ext2gd_free_block(gd, block_pos - gd->base);

    ext2gd_put(gd);
    return 0;
}

static int
__free_blk_recusrive(struct v_superblock *vsb, struct ext2_inode* inode, 
                     unsigned int ind_tab, int level, int max_level)
{
    if (!ind_tab) {
        return 0;
    }

    if (unlikely(level >= max_level)) {
        return 0;
    }

    int errno = 0;
    bbuf_t buf;
    unsigned int* tab;
    unsigned int max_ents, blk;

    buf = fsblock_get(vsb, ext2_datablock(vsb, ind_tab));
    if (blkbuf_errbuf(buf)) {
        return EIO;
    }
    
    tab = (unsigned int*)blkbuf_data(buf);
    max_ents = 1 << inode->inds_lgents;
    for (unsigned int i = 0; i < max_ents; i++) 
    {
        blk = tab[i];
        if (!blk) {
            continue;
        }

        if (level == max_level - 1) {
            __free_block_at(vsb, blk);
            continue;
        }

        errno = __free_blk_recusrive(vsb, inode, blk, level + 1, max_level);
        if (errno) {
            break;
        }

        tab[i] = 0;
    }

    fsblock_dirty(buf);
    return errno;
}

int
ext2ino_free(struct v_superblock* vsb, struct ext2_inode* inode)
{
    int errno = 0;
    struct ext2b_inode* b_ino;
    struct ext2_sbinfo* sb;

    sb = EXT2_SB(vsb);
    b_ino = inode->ino;

    assert_fs(b_ino->i_lnk_cnt > 0);

    fsblock_dirty(inode->buf);

    b_ino->i_lnk_cnt--;
    if (b_ino->i_lnk_cnt >= 1) {
        return 0;
    }

    int i, j = 0;
    for (i = 0; i < 12 && !errno; i++, j++) {
        errno = __free_block_at(vsb, b_ino->i_block.directs[i]);
    }

    for (i = 0; i < 3 && !errno; i++, j++) {
        int block = b_ino->i_block.inds[i];
        errno = __free_blk_recusrive(vsb, inode, block, 0, i + 1);
    }

    for (i = 0; i < j; i++) {
        b_ino->i_block_arr[i] = 0;
    }

    return errno;
}

static void
__update_inode_access_metadata(struct ext2b_inode* b_ino, 
                        struct v_inode* inode)
{
    b_ino->i_ctime = inode->ctime;
    b_ino->i_atime = inode->atime;
    b_ino->i_mtime = inode->mtime;
}

static inline void
__update_inode_size(struct ext2_inode* e_ino, unsigned int size)
{
    struct ext2b_inode* b_ino;

    b_ino = e_ino->ino;
    b_ino->i_size  = size;

    b_ino->i_blocks = ICEIL(size, 512);
    b_ino->i_blocks += e_ino->indirect_blocks;
}

int
ext2ino_make(struct v_superblock* vsb, unsigned int itype, 
             struct ext2_inode* hint, struct v_inode** out)
{
    int errno = 0;
    struct ext2_inode* e_ino;
    struct ext2b_inode* b_ino;
    struct v_inode* inode;

    errno = ext2ino_alloc(vsb, hint, &e_ino);
    if (errno) {
        return errno;
    }

    b_ino = e_ino->ino;
    inode = vfs_i_alloc(vsb);
    
    __ext2ino_fill_common(inode, e_ino->ino_id);

    __update_inode_access_metadata(b_ino, inode);
    b_ino->i_mode  = __translate_vfs_itype(itype);

    fsapi_inode_settype(inode, itype);
    fsapi_inode_complete(inode, e_ino);

    *out = inode;
    return errno;
}

int
ext2_create(struct v_inode* this, struct v_dnode* dnode, unsigned int itype)
{
    int errno;
    struct v_inode* created;
    
    errno = ext2ino_make(this->sb, itype, EXT2_INO(this), &created);
    if (errno) {
        return errno;
    }

    return ext2_link(created, dnode);
}

int
ext2_link(struct v_inode* this, struct v_dnode* new_name)
{
    int errno = 0;
    struct v_inode* parent;
    struct ext2_inode* e_ino;
    struct ext2_dnode* e_dno;
    struct ext2b_dirent dirent;
    
    e_ino  = EXT2_INO(this);
    parent = fsapi_dnode_parent(new_name);

    ext2dr_setup_dirent(&dirent, new_name);
    ext2ino_linkto(e_ino, &dirent);
    
    errno = ext2dr_insert(parent, &dirent, &e_dno);
    if (errno) {
        goto done;
    }

    new_name->data = e_dno;
    vfs_assign_inode(new_name, this);

done:
    return errno;
}

int
ext2_unlink(struct v_inode* this, struct v_dnode* name)
{
    int errno = 0;
    struct ext2_inode* e_ino;
    struct ext2_dnode* e_dno;

    e_ino = EXT2_INO(this);
    e_dno = EXT2_DNO(name);

    assert_fs(e_dno);
    assert_fs(e_dno->self.dirent->inode == e_ino->ino_id);

    errno = ext2dr_remove(e_dno);
    if (errno) {
        return errno;
    }

    return ext2ino_free(this->sb, e_ino);
}

void
ext2ino_update(struct v_inode* inode)
{
    struct ext2_inode* e_ino;
    
    e_ino = EXT2_INO(inode);
    __update_inode_access_metadata(e_ino->ino, inode);
    __update_inode_size(e_ino, inode->fsize);

    fsblock_dirty(e_ino->buf);
}

/* ******************* Data Blocks ******************* */


/**
 * @brief Walk the indrection chain given the position of data block
 *        relative to the inode. Upon completed, walk_state will be
 *        populated with result. On error, walk_state is untouched.
 * 
 *        Note, the result will always be one above the stopping level. 
 *        That means, if your pos is pointed directly to file-content block
 *        (i.e., a leaf block), then the state is the indirect block that
 *        containing the ID of that leaf block.
 *        
 *        If `resolve` is set, it will resolve any absence encountered
 *        during the walk by allocating and chaining indirect block.
 *        It require the file system is mounted writable.
 * 
 * @param inode 
 * @param pos 
 * @param state 
 * @param resolve 
 * @return int 
 */
static int
__walk_indirects(struct v_inode* inode, unsigned int pos,
                 struct walk_state* state, bool resolve)
{
    int errno;
    int inds, stride, shifts, level;
    unsigned int *slotref, index, next, mask;
    struct ext2_inode* e_inode;
    struct ext2b_inode* b_inode;
    struct v_superblock* vsb;
    bbuf_t table, next_table;

    e_inode = EXT2_INO(inode);
    b_inode = e_inode->ino;
    vsb = inode->sb;
    level = 0;
    resolve = resolve && !EXT2_SB(vsb)->read_only;

    if (pos < 12) {
        index = pos;
        slotref = &b_inode->i_block_arr[pos];
        table = fsblock_take(e_inode->buf);
        inds = 0;
        goto _return;
    }

    pos -= 12;
    stride = e_inode->inds_lgents;
    if (!(pos >> stride)) {
        inds = 1;
    }
    else if (!(pos >> (stride * 2))) {
        inds = 2;
    }
    else if (!(pos >> (stride * 3))) {
        inds = 3;
    }
    else {
        fail("unrealistic block pos");
    }

    // bTLB cache the last level indirect block
    if ((table = __btlb_hit(inode, pos))) {
        level = inds;
        index = pos & ((1 << stride) - 1);
        slotref = &block_buffer(table, u32_t)[index];
        goto _return;
    }

    shifts = stride * (inds - 1);
    mask = ((1 << stride) - 1) << shifts;

    slotref = &b_inode->i_block.inds[inds - 1];
    table = fsblock_take(e_inode->buf);

    for (; level < inds; level++)
    {
        next = *slotref;
        if (!next) {
            if (!resolve) {
                goto _return;
            }

            if ((errno = ext2db_alloc(inode, &next_table))) {
                return errno;
            }

            e_inode->indirect_blocks++;
            *slotref = fsblock_id(next_table);
            fsblock_dirty(table);
        }
        else {
            next_table = fsblock_get(vsb, next);
        }

        fsblock_put(table);
        table = next_table;

        if (blkbuf_errbuf(table)) {
            return EIO;
        }

        assert(shifts >= 0);

        index = (pos & mask) >> shifts;
        slotref = &block_buffer(table, u32_t)[index];

        shifts -= stride;
        mask  = mask >> stride;
    }

    __btlb_insert(inode, pos, table);

_return:
    assert(blkbuf_refcounts(table) >= 1);
    assert_fs(table);
    assert_fs(slotref);

    state->slot_ref = slotref;
    state->table = table;
    state->level = level;
    state->indirections = inds;

    return 0;
}

bbuf_t
ext2db_get(struct v_inode* inode, unsigned int data_pos)
{
    int errno;
    unsigned int blkid;
    struct walk_state state;

    errno = __walk_indirects(inode, data_pos, &state, false);
    if (errno) {
        return (bbuf_t)INVL_BUFFER;
    }

    fsblock_put(state.table);

    blkid = *state.slot_ref;
    if (!blkid) {
        return NULL;
    }

    return fsblock_get(inode->sb, blkid);
}

int
ext2db_acquire(struct v_inode* inode, unsigned int data_pos, bbuf_t* out)
{
    int errno = 0;
    bbuf_t buf;
    unsigned int block_id;
    struct walk_state state;

    errno = __walk_indirects(inode, data_pos, &state, true);
    if (errno) {
        return errno;
    }

    block_id = *state.slot_ref;
    if (block_id) {
        buf = fsblock_get(inode->sb, block_id);
        goto done;
    }

    errno = ext2db_alloc(inode, &buf);
    if (errno) {
        return errno;
    }

    *state.slot_ref = fsblock_id(buf);
    fsblock_dirty(state.table);

done:
    fsblock_put(state.table);

    if (blkbuf_errbuf(buf)) {
        return EIO;
    }

    *out = buf;
    return 0;
}

int
ext2db_alloc(struct v_inode* inode, bbuf_t* out)
{
    int free_ino_idx;
    struct ext2_gdesc* gd;
    struct ext2_inode* e_inode;
    struct v_superblock* vsb;

    free_ino_idx = ALLOC_FAIL;
    e_inode = EXT2_INO(inode);
    vsb = inode->sb;

    gd = e_inode->blk_grp;
    free_ino_idx = ext2gd_alloc_block(gd);

    // locality alloc failed, try entire fs
    if (!valid_bmp_slot(free_ino_idx)) {
        free_ino_idx = ext2db_alloc_slot(vsb, &gd);
    }

    if (!valid_bmp_slot(free_ino_idx)) {
        return EDQUOT;
    }

    free_ino_idx += gd->base;
    free_ino_idx = ext2_datablock(vsb, free_ino_idx);
    free_ino_idx = to_ext2ino_id(free_ino_idx);
    
    bbuf_t buf = fsblock_get(vsb, free_ino_idx);
    if (blkbuf_errbuf(buf)) {
        return EIO;
    }

    *out = buf;
    return 0;
}

void
ext2db_free_pos(struct v_inode* inode, unsigned int block_pos)
{
    struct ext2_inode* e_inode;
    struct ext2_gdesc* gd;

    e_inode = EXT2_INO(inode);
    gd = e_inode->blk_grp;

    assert(block_pos >= gd->base);
    
    block_pos -= gd->base;

    ext2gd_free_block(gd, block_pos);
}

int
ext2db_free(struct v_inode* inode, bbuf_t buf)
{
    assert(blkbuf_not_shared(buf));

    ext2db_free_pos(inode, blkbuf_id(buf));
    fsblock_put(buf);

    return 0;
}