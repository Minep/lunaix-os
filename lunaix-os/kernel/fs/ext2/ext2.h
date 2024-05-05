#ifndef __LUNAIX_EXT2_H
#define __LUNAIX_EXT2_H

#include <lunaix/fs/api.h>
#include <lunaix/types.h>
#include <lunaix/ds/llist.h>
#include <lunaix/ds/hashtable.h>
#include <lunaix/ds/lru.h>

typedef unsigned int ext2_bno_t;

struct ext2b_super {
    u32_t s_ino_cnt;
    u32_t s_blk_cnt;
    u32_t s_r_blk_cnt;
    u32_t s_free_blk_cnt;
    u32_t s_free_ino_cnt;
    u32_t s_first_data_cnt;

    u32_t s_log_blk_size;
    u32_t s_log_frg_size;

    u32_t s_blk_per_grp;
    u32_t s_frg_per_grp;
    u32_t s_ino_per_grp;

    u32_t s_mtime;
    u32_t s_wtime;
    
    u16_t s_mnt_cnt;
    u16_t s_magic;
    u16_t s_state;
    u16_t s_error;
    u16_t s_minor_rev;

    u32_t s_last_check;
    u32_t s_checkinterval;
    u32_t s_creator_os;
    u32_t s_rev;
    
    u16_t s_def_resuid;
    u16_t s_def_resgid;

    // EXT2_DYNAMIC_REV

    struct {
        u32_t s_first_ino;
        u16_t s_ino_size;
        u16_t s_blkgrp_nr;
        u32_t s_optional_feat;
        u32_t s_required_feat;
        u32_t s_ro_feat;
        u8_t s_uuid[16];
        u8_t s_volname[16];
        u8_t s_last_mnt[64];
        u32_t s_algo_bmp;
    } compact;

} compact;

struct ext2b_gdesc
{
    u32_t bg_blk_map;
    u32_t bg_ino_map;
    u32_t bg_ino_tab;

    u16_t bg_free_blk_cnt;
    u16_t bg_free_ino_cnt;
    u16_t bg_used_dir_cnt;
    u16_t bg_pad;
} align(32) compact;

struct ext2b_inode
{
    u16_t i_mode;
    u16_t i_uid;
    u32_t i_size;

    u32_t i_atime;
    u32_t i_ctime;
    u32_t i_mtime;
    u32_t i_dtime;

    u16_t i_gid;
    u16_t i_lnk_cnt;

    u32_t i_blocks;
    u32_t i_flags;
    u32_t i_osd1;

    struct 
    {
        u32_t directs[12];  // directum
        u32_t ind1;         // prima indirecta
        u32_t ind23[2];     // secunda et tertia indirecta
    } i_block;

    u32_t i_gen;
    u32_t i_file_acl;
    u32_t i_dir_acl;
    u32_t i_faddr;

    u8_t i_osd2[12];
} compact;

struct ext2b_dirent 
{
    u32_t inode;
    u16_t rec_len;
    u8_t name_len;
    u8_t file_type;
    u8_t name[256];
} align(4) compact;
#define EXT2_DRE(v_dnode) (fsapi_impl_data(v_dnode, struct ext2b_dirent))

struct ext2_sbinfo 
{
    /**
     * @brief 
     * offset to inode table (in terms of blocks) within each block group.
     * to account the difference of backup presence between rev 0/1
     */
    int ino_tab_len;

    bool read_only;
    unsigned int block_size;
    unsigned int nr_gdesc_pb;
    unsigned int nr_gdesc;

    struct device* bdev;
    struct v_superblock* vsb;
    
    struct ext2b_super raw;
    bbuf_t* gdt_frag;
};
#define EXT2_SB(vsb) (fsapi_impl_data(vsb, struct ext2_sbinfo))

/*
    Indriection Block Translation Look-aside Buffer
    
    Provide a look-aside buffer for all last-level indirect block 
    that is at least two indirection away.

    For 4KiB block size:
        16 sets, 256 ways, capacity 4096 blocks
*/

#define BTLB_SETS        16
struct ext2_btlb_entry
{
    unsigned int tag;
    bbuf_t block;
};

struct ext2_btlb
{
    struct ext2_btlb_entry buffer[BTLB_SETS];
};

struct ext2_inode
{
    bbuf_t inotab;
    unsigned int inds_lgents;
    unsigned int nr_blocks;
    struct ext2b_inode* ino;
    struct ext2_btlb* btlb;

    // prefetched block for 1st order of indirection
    bbuf_t ind_ord1;
};
#define EXT2_INO(v_inode) (fsapi_impl_data(v_inode, struct ext2_inode))

/**
 * @brief General purpose iterator for ext2 objects
 * 
 */
struct ext2_iterator
{
    struct v_inode* inode;
    
    union {
        struct ext2b_dirent* dirent;
        void* data;
    };

    union {
        struct {
            bool has_error:1;
        };
        unsigned int flags;
    };

    unsigned int pos;
    unsigned int blk_pos;
    unsigned int block_end;
    bbuf_t sel_buf;
};

struct ext2_file
{
    struct ext2_iterator iter;
    struct ext2_inode* b_ino;
};
#define EXT2_FILE(v_file) (fsapi_impl_data(v_file, struct ext2_file))

static inline unsigned int
ext2_datablock(struct ext2_sbinfo* sb, unsigned int id)
{
    return sb->raw.s_first_data_cnt + id;
}


/* ************   Inodes   ************ */

void
ext2_init_inode(struct v_superblock* vsb, struct v_inode* inode);

int
ext2_get_inode(struct v_superblock* vsb, 
               unsigned int ino, struct ext2_inode** out);

bbuf_t
ext2_get_data_block(struct v_inode* inode, unsigned int data_pos);

int
ext2_fill_inode(struct v_inode* inode, ino_t ino_id);

void
ext2db_itbegin(struct ext2_iterator* iter, struct v_inode* inode);

void
ext2db_itend(struct ext2_iterator* iter);

bool
ext2db_itnext(struct ext2_iterator* iter);

int
ext2db_itffw(struct ext2_iterator* iter, int count);

void
ext2db_itreset(struct ext2_iterator* iter);


/* ************* Iterator ************* */

static inline bool
ext2_iterror(struct ext2_iterator* iter) {
    return iter->has_error;
}

static inline bool
ext2_itcheckbuf(struct ext2_iterator* iter) {
    return !(iter->has_error = blkbuf_errbuf(iter->sel_buf));
}

#define itstate_sel(iter, value)    \
    (ext2_iterror(iter) ? EIO : (value))


/* ************ Block Group ************ */

void
ext2gd_prepare_gdt(struct v_superblock* vsb);

void
ext2gd_release_gdt(struct v_superblock* vsb);

int
ext2gd_desc_at(struct v_superblock* vsb, 
               unsigned int index, struct ext2b_gdesc** out);


/* ************ Directory ************ */

int
ext2dr_lookup(struct v_inode* this, struct v_dnode* dnode);

int
ext2dr_read(struct v_file *file, struct dir_context *dctx);

void
ext2dr_itbegin(struct ext2_iterator* iter, struct v_inode* inode);

void
ext2dr_itend(struct ext2_iterator* iter);

bool
ext2dr_itnext(struct ext2_iterator* iter);

int
ext2dr_itffw(struct ext2_iterator* iter, int count);

void
ext2dr_itreset(struct ext2_iterator* iter);

int
ext2dr_open(struct v_inode* this, struct v_file* file);

int
ext2dr_seek(struct v_file* file, size_t offset);


/* ************   Files   ************ */

int
ext2_open_inode(struct v_inode* this, struct v_file* file);

int
ext2_close_inode(struct v_file* file);

int
ext2_sync_inode(struct v_file* file);

int
ext2_inode_read(struct v_inode *inode, void *buffer, size_t len, size_t fpos);

int
ext2_inode_read_page(struct v_inode *inode, void *buffer, size_t fpos);

int
ext2_inode_write(struct v_inode *inode, void *buffer, size_t len, size_t fpos);

int
ext2_inode_write_page(struct v_inode *inode, void *buffer, size_t fpos);

#endif /* __LUNAIX_EXT2_H */
