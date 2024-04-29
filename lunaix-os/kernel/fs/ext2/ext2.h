#ifndef __LUNAIX_EXT2_H
#define __LUNAIX_EXT2_H

#include <lunaix/fs/api.h>
#include <lunaix/types.h>
#include <lunaix/ds/llist.h>
#include <lunaix/ds/hashtable.h>
#include <lunaix/ds/lru.h>

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
    u16_t i_linl_cnt;

    u32_t i_blocks;
    u32_t i_flags;
    u32_t i_osd1;

    struct {
        union 
        {
            u32_t directs[12];
            u32_t ind1;
            u32_t ind2;
            u32_t ind3;
        } ind_ord0;             // directum
        union 
        {
            u32_t directs[15];
        } ind_ord1;             // first order indirection (prima indirecta)
        union 
        {
            u32_t reford_1[15];
        } ind_ord2;             // second order indirection (duplex indirectum)
        union 
        {
            u32_t reford_2[15];
        } ind_ord3;             // third order indirection (trina indirecta)
    } i_block;

    u32_t i_gen;
    u32_t i_file_acl;
    u32_t i_dir_acl;
    u32_t i_faddr;

    u8_t i_osd2[12];
} compact;

struct ext2_dirent 
{
    u32_t inode;
    u16_t rec_len;
    u8_t name_len;
    u8_t file_type;
    u8_t name[256];
} align(4) compact;
#define EXT2_DRE(v_dnode) (fsapi_impl_data(v_dnode, struct ext2_dirent))

struct ext2_sbinfo 
{
    /**
     * @brief 
     * offset to inode table (in terms of blocks) within each block group.
     * to account the difference of backup presence between rev 0/1
     */
    int ino_tab_off;

    bool read_only;
    size_t block_size;
    struct device* bdev;
    struct v_superblock* vsb;
    struct ext2b_super raw;
};
#define EXT2_SB(vsb) (fsapi_impl_data(vsb, struct ext2_sbinfo))

struct ext2_ino
{
    struct ext2b_inode* raw;
};
#define EXT2_INO(v_inode) (fsapi_impl_data(v_inode, struct ext2_ino))


void
ext2_init_inode(struct v_superblock* vsb, struct v_inode* inode);

#endif /* __LUNAIX_EXT2_H */
