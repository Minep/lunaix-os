#include <lunaix/fs/api.h>
#include <lunaix/mm/valloc.h>

#include "ext2.h"

#define IMODE_IFSOCK    0xc000
#define IMODE_IFLNK     0xA000
#define IMODE_IFREG     0x8000
#define IMODE_IFBLK     0x6000
#define IMODE_IFDIR     0x4000
#define IMODE_IFCHR     0x2000
#define IMODE_IFFIFO    0x1000


static struct v_inode_ops ext2_inode_ops = {
    .dir_lookup = ext2dr_lookup,
    .open  = ext2_open_inode,
    .mkdir = NULL,
    .rmdir = NULL,
    .read_symlink = NULL,
    .set_symlink  = NULL,
    .rename = NULL
};

static struct v_file_ops ext2_file_ops = {
    .close = ext2_close_inode,
    
    .read = ext2_inode_read,
    .read_page = ext2_inode_read_page,
    
    .write = ext2_inode_write,
    .write_page = ext2_inode_write_page,
    
    .readdir = ext2dr_read,
    .seek = NULL,
    .sync = ext2_sync_inode
};

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
        if (btlb->buffer[i].tag) {
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
    cap_sel = hash_32(blkid, ILOG2(BTLB_SETS));
    btlbe = &btlb->buffer[cap_sel];

    fsblock_put(btlbe->block);

found:
    btlbe->tag = blkid & ~((1 << e_inode->inds_lgents) - 1);
    btlbe->block = buf;
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
    in_tag = blkid & ~((1 << e_inode->inds_lgents) - 1);

    for (int i = 0; i < BTLB_SETS; i++)
    {
        btlbe = &btlb->buffer[i];
        if (btlbe->tag == in_tag) {
            return blkbuf_refonce(btlbe->block);
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
        if (btlbe->tag) {
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

    vfree(e_inode->btlb);
    vfree(e_inode);
}

int
ext2ino_fill(struct v_inode* inode, ino_t ino_id)
{
    struct ext2_sbinfo* sb;
    struct ext2_inode* ext2inode;
    struct ext2b_inode* b_ino;
    struct v_superblock* vsb;
    unsigned int type = VFS_IFFILE;
    int errno = 0;

    vsb = inode->sb;
    sb = EXT2_SB(vsb);

    if ((errno = ext2ino_get(vsb, ino_id, &ext2inode))) {
        return errno;
    }
    b_ino = ext2inode->ino;
    
    fsapi_inode_setid(inode, ino_id, ino_id);
    fsapi_inode_setsize(inode, b_ino->i_size);
    
    fsapi_inode_settime(inode, b_ino->i_ctime, 
                               b_ino->i_mtime, 
                               b_ino->i_atime);
    
    fsapi_inode_setfops(inode, &ext2_file_ops);
    fsapi_inode_setops(inode, &ext2_inode_ops);
    fsapi_inode_setdector(inode, ext2_destruct_inode);

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

    fsapi_inode_complete(inode, ext2inode);

    return errno;
}

static struct ext2_inode*
__create_inode(struct v_superblock* vsb, struct ext2_gdesc* gd, int inode_idx)
{
    bbuf_t ino_tab;
    struct ext2_sbinfo* sb;
    struct ext2b_inode* b_inode;
    struct ext2_inode* inode;
    unsigned int ino_tab_sel, ino_tab_off, 
                 tab_partlen, ind_ents;

    sb = gd->sb;
    tab_partlen = sb->block_size / sb->raw.s_ino_size;
    ino_tab_sel = inode_idx / tab_partlen;
    ino_tab_off = inode_idx % tab_partlen;

    ino_tab = fsblock_take(vsb, gd->info->bg_ino_tab + ino_tab_sel);
    if (blkbuf_errbuf(ino_tab)) {
        return NULL;
    }

    b_inode = (struct ext2b_inode*)blkbuf_data(ino_tab);
    
    inode            = valloc(sizeof(*inode));
    inode->btlb      = valloc(sizeof(struct ext2_btlb));
    inode->buf       = ino_tab;
    inode->ino       = &b_inode[ino_tab_off];
    inode->blk_grp   = gd;

    ind_ents = sb->block_size / sizeof(int);
    assert(is_pot(ind_ents));

    inode->inds_lgents = ILOG2(ind_ents);

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
    unsigned int ind_ents;
    int errno = 0;
    
    sb = EXT2_SB(vsb);

    ino -= 1;
    blkgrp_id   = ino / sb->raw.s_ino_per_grp;
    ino_rel_id  = ino % sb->raw.s_ino_per_grp;

    if ((errno = ext2gd_take(vsb, blkgrp_id, &gd))) {
        return errno;
    }

    inode = __create_inode(vsb, gd, ino_rel_id);
    if (!inode) {
        return EIO;
    }

    b_inode = inode->ino;

    if (b_inode->i_block.ind1) {
        inode->ind_ord1 = fsblock_take(vsb, b_inode->i_block.ind1);
        
        if (blkbuf_errbuf(inode->ind_ord1)) {
            vfree(inode->btlb);
            vfree(inode);
            return EIO;
        }
    }

    *out = inode;
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

    // locality alloc failed, try entire fs
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
    gd_index = block_pos / sb->raw.s_blk_per_grp;

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

    buf = fsblock_take(vsb, ext2_datablock(vsb, ind_tab));
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

static inline bbuf_t
__follow_indrect(struct v_inode* inode, bbuf_t buf, unsigned int blkid)
{
    if (unlikely(!buf)) {
        return NULL;
    }

    if (unlikely(blkbuf_errbuf(buf))) {
        return buf;
    }

    blkid = blkid % (1 << EXT2_INO(inode)->inds_lgents);

    unsigned int i = block_buffer(buf, unsigned int)[blkid];
    fsblock_put(buf);

    return i ? fsblock_take(inode->sb, i) : NULL;
}

static bbuf_t
__get_datablock_at(struct v_inode* inode,
                   unsigned int blkid, int ind_order)
{
    struct ext2_inode* e_inode;
    unsigned int ind_block, lg_ents;
    bbuf_t indirect;

    e_inode = EXT2_INO(inode);
    lg_ents = e_inode->inds_lgents;

    if (ind_order == 1) {
        indirect = e_inode->ind_ord1;
        goto done;
    }

    if ((indirect = __btlb_hit(inode, blkid))) {
        goto done;
    }

    ind_order -= 2;
    ind_block = e_inode->ino->i_block.ind23[ind_order];
    if (!ind_block) {
        return NULL;
    }

    indirect = fsblock_take(inode->sb, ind_block);

    if (ind_order)
    {
        blkid >>= lg_ents;
        indirect = __follow_indrect(inode, indirect, blkid);
    }

    if (indirect) {
        __btlb_insert(inode, blkid, indirect);
    }
    
done:
    return __follow_indrect(inode, indirect, blkid);
}

bbuf_t
ext2db_get(struct v_inode* inode, unsigned int data_pos)
{
    struct ext2_inode* e_inode;
    struct ext2b_inode* b_inode;
    unsigned int blkid;
    unsigned int lg_ents;

    e_inode = EXT2_INO(inode);
    b_inode = e_inode->ino;
    lg_ents = e_inode->inds_lgents;

    assert(data_pos < 15);

    if (data_pos < 13) {
        return fsblock_take(inode->sb, b_inode->i_block.directs[data_pos]);
    }
    
    blkid = data_pos - 12;

    int i = 0;
    while (blkid) {
        blkid >>= lg_ents;
        i++;
    }

    return __get_datablock_at(inode, data_pos, i);
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
    
    bbuf_t buf = fsblock_take(vsb, free_ino_idx);
    if (blkbuf_errbuf(buf)) {
        return EIO;
    }

    *out = buf;
    return 0;
}

int
ext2db_free(struct v_inode* inode, bbuf_t buf)
{
    struct ext2_inode* e_inode;
    struct ext2_gdesc* gd;
    unsigned int block_pos;

    block_pos = blkbuf_id(buf);
    e_inode = EXT2_INO(inode);
    gd = e_inode->blk_grp;

    assert(blkbuf_not_shared(buf));
    assert(block_pos >= gd->base);
    
    block_pos -= gd->base;

    ext2gd_free_block(gd, block_pos);
    fsblock_put(buf);

    return 0;
}