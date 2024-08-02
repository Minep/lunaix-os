#ifndef __LUNAIX_EXT2_H
#define __LUNAIX_EXT2_H

#include <lunaix/fs/api.h>
#include <lunaix/types.h>
#include <lunaix/ds/llist.h>
#include <lunaix/ds/hashtable.h>
#include <lunaix/ds/lru.h>

typedef unsigned int ext2_bno_t;

#define ext2_aligned            compact align(4)
#define to_ext2ino_id(fsblock_id)       ((fsblock_id) + 1)
#define to_fsblock_id(ext2_ino)         ((ext2_ino) - 1)

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
    u16_t s_max_mnt_cnt;
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

} ext2_aligned;

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

    union {
        struct 
        {
            u32_t directs[12];  // directum
            union {
                struct {
                    u32_t ind1;         // prima indirecta
                    u32_t ind23[2];     // secunda et tertia indirecta
                } ext2_aligned;
                u32_t inds[3];
            };
        } ext2_aligned i_block;
        u32_t i_block_arr[15];
    };

    u32_t i_gen;
    u32_t i_file_acl;
    u32_t i_dir_acl;
    u32_t i_faddr;

    u8_t i_osd2[12];
} ext2_aligned;

struct ext2b_dirent 
{
    u32_t inode;
    u16_t rec_len;
    u8_t name_len;
    u8_t file_type;
    char name[256];
} ext2_aligned;
#define EXT2_DRE(v_dnode) (fsapi_impl_data(v_dnode, struct ext2b_dirent))


#define GDESC_INO_SEL 0
#define GDESC_BLK_SEL 1

#define GDESC_FREE_LISTS                        \
    union {                                     \
        struct {                                \
            struct llist_header free_grps_ino;  \
            struct llist_header free_grps_blk;  \
        };                                      \
        struct llist_header free_list_sel[2];   \
    }

#define check_gdesc_type_sel(sel)                               \
    assert_msg(sel == GDESC_INO_SEL || sel == GDESC_BLK_SEL,    \
                "invalid type_sel");

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
    
    struct ext2b_super* raw;
    bbuf_t* gdt_frag;
    struct bcache gd_caches;
    
    bbuf_t buf;

    struct {
        struct llist_header gds;
        GDESC_FREE_LISTS;
    };
};
#define EXT2_SB(vsb) (fsapi_impl_data(vsb, struct ext2_sbinfo))


struct ext2_bmp
{
    bbuf_t raw;
    u8_t* bmp;
    unsigned int nr_bytes;
    int next_free;
};

struct ext2_gdesc
{
    struct llist_header groups;
    GDESC_FREE_LISTS;

    union {
        struct {
            struct ext2_bmp ino_bmp;
            struct ext2_bmp blk_bmp;
        };
        struct ext2_bmp bmps[2];
    };

    unsigned int base;
    unsigned int ino_base;

    struct ext2b_gdesc* info;
    struct ext2_sbinfo* sb;
    bbuf_t buf;
    bcobj_t cache_ref;
};

/*
    Indriection Block Translation Look-aside Buffer
    
    Provide a look-aside buffer for all last-level indirect block 
    that is at least two indirection away.

    For 4KiB block size:
        16 sets, 256 ways, capacity 4096 blocks
*/

struct ext2_btlb_entry
{
    unsigned int tag;
    bbuf_t block;
};

#define BTLB_SETS        16
struct ext2_btlb
{
    struct ext2_btlb_entry buffer[BTLB_SETS];
};

struct ext2_inode
{
    bbuf_t buf;                  // partial inotab that holds this inode
    unsigned int inds_lgents;       // log2(# of block in an indirection level)
    unsigned int ino_id;
    unsigned int indirect_blocks;

    struct ext2b_inode* ino;        // raw ext2 inode
    struct ext2_btlb* btlb;         // block-TLB
    struct ext2_gdesc* blk_grp;     // block group

    union {
        struct {
            /*
                dirent fragmentation degree, we will perform
                full reconstruction on dirent table when this goes too high.
            */
            unsigned int dir_fragdeg;
        }; 
    };

    // prefetched block for 1st order of indirection
    bbuf_t ind_ord1;
    char* symlink;
};
#define EXT2_INO(v_inode) (fsapi_impl_data(v_inode, struct ext2_inode))

struct ext2_dnode_sub
{
    bbuf_t buf;
    struct ext2b_dirent* dirent;
};

struct ext2_dnode
{
    struct ext2_dnode_sub self;
    struct ext2_dnode_sub prev;
};
#define EXT2_DNO(v_dnode) (fsapi_impl_data(v_dnode, struct ext2_dnode))


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
    unsigned int blksz;
    unsigned int end_pos;
    bbuf_t sel_buf;
};

struct ext2_file
{
    struct ext2_iterator iter;
    struct ext2_inode* b_ino;
};
#define EXT2_FILE(v_file) (fsapi_impl_data(v_file, struct ext2_file))

static inline unsigned int
ext2_datablock(struct v_superblock* vsb, unsigned int id)
{
    return EXT2_SB(vsb)->raw->s_first_data_cnt + id;
}


/* ************   Inodes   ************ */

void
ext2ino_init(struct v_superblock* vsb, struct v_inode* inode);

int
ext2ino_get(struct v_superblock* vsb, 
               unsigned int ino, struct ext2_inode** out);

int
ext2ino_fill(struct v_inode* inode, ino_t ino_id);

int
ext2ino_make(struct v_superblock* vsb, unsigned int itype, 
             struct ext2_inode* hint, struct v_inode** out);

void
ext2ino_update(struct v_inode* inode);

static inline void
ext2ino_linkto(struct ext2_inode* e_ino, struct ext2b_dirent* dirent)
{
    dirent->inode = e_ino->ino_id;
    e_ino->ino->i_lnk_cnt++;
    fsblock_dirty(e_ino->buf);
}

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


/**
 * @brief Get the data block at given data pos associated with the
 *        inode, return NULL if not present
 * 
 * @param inode 
 * @param data_pos 
 * @return bbuf_t 
 */
bbuf_t
ext2db_get(struct v_inode* inode, unsigned int data_pos);

/**
 * @brief Get the data block at given data pos associated with the
 *        inode, allocate one if not present.
 * 
 * @param inode 
 * @param data_pos 
 * @param out 
 * @return int 
 */
int
ext2db_acquire(struct v_inode* inode, unsigned int data_pos, bbuf_t* out);

void
ext2db_free_pos(struct v_inode* inode, unsigned int block_pos);

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
    (ext2_iterror(iter) ? EIO : (int)(value))


/* ************ Block Group ************ */

void
ext2gd_prepare_gdt(struct v_superblock* vsb);

void
ext2gd_release_gdt(struct v_superblock* vsb);

int
ext2gd_take(struct v_superblock* vsb, 
               unsigned int index, struct ext2_gdesc** out);

static inline void
ext2gd_put(struct ext2_gdesc* gd) {
    bcache_return(gd->cache_ref);
}


/* ************ Directory ************ */

int
ext2dr_lookup(struct v_inode* this, struct v_dnode* dnode);

int
ext2dr_read(struct v_file *file, struct dir_context *dctx);

void
ext2dr_itbegin(struct ext2_iterator* iter, struct v_inode* inode);

void
ext2dr_itend(struct ext2_iterator* iter);

static inline bool
ext2dr_itdrain(struct ext2_iterator* iter)
{
    return iter->pos > iter->end_pos;
}

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

int
ext2dr_insert(struct v_inode* this, struct ext2b_dirent* dirent,
              struct ext2_dnode** e_dno_out);

int
ext2dr_remove(struct ext2_dnode* e_dno);

int
ext2_rmdir(struct v_inode* parent, struct v_dnode* dnode);

int
ext2_mkdir(struct v_inode* parent, struct v_dnode* dnode);

int
ext2_rename(struct v_inode* from_inode, struct v_dnode* from_dnode,
            struct v_dnode* to_dnode);

void
ext2dr_setup_dirent(struct ext2b_dirent* dirent, struct v_dnode* dnode);


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

int
ext2_seek_inode(struct v_file* file, size_t offset);

int
ext2_create(struct v_inode* this, struct v_dnode* dnode, unsigned int itype);

int
ext2_link(struct v_inode* this, struct v_dnode* new_name);

int
ext2_unlink(struct v_inode* this, struct v_dnode* name);

int
ext2_get_symlink(struct v_inode *this, const char **path_out);

int
ext2_set_symlink(struct v_inode *this, const char *target);

/* ***********   Bitmap   *********** */

void
ext2bmp_init(struct ext2_bmp* e_bmp, bbuf_t bmp_buf, unsigned int nr_bits);

bool
ext2bmp_check_free(struct ext2_bmp* e_bmp);

int
ext2bmp_alloc_one(struct ext2_bmp* e_bmp);

void
ext2bmp_free_one(struct ext2_bmp* e_bmp, unsigned int pos);

void
ext2bmp_discard(struct ext2_bmp* e_bmp);

/* ***********   Allocations   *********** */

#define ALLOC_FAIL -1

static inline bool
valid_bmp_slot(int slot)
{
    return slot != ALLOC_FAIL;
}

int
ext2gd_alloc_slot(struct ext2_gdesc* gd, int type_sel);

void
ext2gd_free_slot(struct ext2_gdesc* gd, int type_sel, int slot);

static inline int
ext2gd_alloc_inode(struct ext2_gdesc* gd) 
{
    return ext2gd_alloc_slot(gd, GDESC_INO_SEL);
}

static inline int
ext2gd_alloc_block(struct ext2_gdesc* gd) 
{
    return ext2gd_alloc_slot(gd, GDESC_BLK_SEL);
}

static inline void
ext2gd_free_inode(struct ext2_gdesc* gd, int slot) 
{
    ext2gd_free_slot(gd, GDESC_INO_SEL, slot);
}

static inline void
ext2gd_free_block(struct ext2_gdesc* gd, int slot) 
{
    ext2gd_free_slot(gd, GDESC_BLK_SEL, slot);
}


/**
 * @brief Allocate a free inode
 * 
 * @param vsb 
 * @param hint locality hint
 * @param out 
 * @return int 
 */
int
ext2ino_alloc(struct v_superblock* vsb, 
                 struct ext2_inode* hint, struct ext2_inode** out);

/**
 * @brief Allocate a free data block
 * 
 * @param inode inode where the data block goes, also used as locality hint
 * @return bbuf_t 
 */
int
ext2db_alloc(struct v_inode* inode, bbuf_t* out);

/**
 * @brief free an inode
 * 
 * @param vsb 
 * @param hint locality hint
 * @param out 
 * @return int 
 */
int
ext2ino_free(struct v_superblock* vsb, 
                 struct ext2_inode* inode);

/**
 * @brief Free a data block
 * 
 * @param inode inode where the data block goes, also used as locality hint
 * @return bbuf_t 
 */
int
ext2db_free(struct v_inode* inode, bbuf_t buf);

int
ext2ino_alloc_slot(struct v_superblock* vsb, struct ext2_gdesc** gd_out);

int
ext2db_alloc_slot(struct v_superblock* vsb, struct ext2_gdesc** gd_out);



extern bcache_zone_t gdesc_bcache_zone;

#endif /* __LUNAIX_EXT2_H */
