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
    *iter = (struct ext2_iterator){
        .pos = 0,
        .inode = inode,
        .blksz = inode->sb->blksize
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
ext2_init_inode(struct v_superblock* vsb, struct v_inode* inode)
{
    // Perhaps we dont need this at all...
}

void
ext2_destruct_inode(struct v_inode* inode)
{
    struct ext2_inode* e_inode;

    e_inode = EXT2_INO(inode);

    // FIXME for async inode freeing after unmount, 
    //       sb is not available in current design.

    __btlb_flushall(inode);
    fsblock_put(e_inode->ind_ord1);
    fsblock_put(e_inode->inotab);

    vfree(e_inode->btlb);
    vfree(e_inode);
}

int
ext2_fill_inode(struct v_inode* inode, ino_t ino_id)
{
    struct ext2_sbinfo* sb;
    struct ext2_inode* ext2inode;
    struct ext2b_inode* b_ino;
    struct v_superblock* vsb;
    unsigned int type = VFS_IFFILE;
    int errno = 0;

    vsb = inode->sb;
    sb = EXT2_SB(vsb);

    if ((errno = ext2_get_inode(vsb, ino_id, &ext2inode))) {
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

int
ext2_get_inode(struct v_superblock* vsb, 
               unsigned int ino, struct ext2_inode** out)
{
    struct ext2_sbinfo* sb;
    struct ext2_inode* inode;
    struct ext2b_inode* b_inode;
    struct ext2b_gdesc* gd;
    unsigned int blkgrp_id, ino_rel_id;
    unsigned int ino_tab_sel, ino_tab_off, tab_partlen;
    unsigned int ind_ents;
    int errno = 0;
    bbuf_t ino_tab;
    
    sb = EXT2_SB(vsb);

    ino -= 1;
    blkgrp_id   = ino / sb->raw.s_ino_per_grp;
    ino_rel_id  = ino % sb->raw.s_ino_per_grp;

    tab_partlen = sb->block_size / sb->raw.s_ino_size;
    ino_tab_sel = ino_rel_id / tab_partlen;
    ino_tab_off = ino_rel_id % tab_partlen;

    if ((errno = ext2gd_desc_at(vsb, blkgrp_id, &gd))) {
        return errno;
    }

    ino_tab = fsblock_take(vsb, gd->bg_ino_tab + ino_tab_sel);
    if (blkbuf_errbuf(ino_tab)) {
        return EIO;
    }

    b_inode = (struct ext2b_inode*)blkbuf_data(ino_tab);
    
    inode            = valloc(sizeof(*inode));
    inode->btlb      = valloc(sizeof(struct ext2_btlb));
    inode->inotab    = ino_tab;
    inode->ino       = &b_inode[ino_tab_off];
    inode->nr_blocks = b_inode->i_blocks / (sb->block_size / 512);

    ind_ents = sb->block_size / sizeof(int);
    assert(is_pot(ind_ents));

    inode->inds_lgents = ILOG2(ind_ents);

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