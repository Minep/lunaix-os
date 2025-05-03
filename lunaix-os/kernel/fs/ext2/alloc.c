#include "ext2.h"

static inline unsigned int
__ext2_global_slot_alloc(struct v_superblock* vsb, int type_sel, 
                         struct ext2_gdesc** gd_out)
{
    int alloc;
    struct ext2_sbinfo* sb;
    struct ext2_gdesc *pos;
    struct llist_header *header;
    
    alloc = ALLOC_FAIL;
    sb = EXT2_SB(vsb);

    ext2sb_lock(sb);
    header = &sb->free_list_sel[type_sel];

    // we have used up all avaliable inodes/blocks
    if (llist_empty(header))
        goto done;
    
    if (type_sel == GDESC_INO_SEL) {
        pos = list_entry(header->next, struct ext2_gdesc, free_grps_ino);
    } 
    else {
        pos = list_entry(header->next, struct ext2_gdesc, free_grps_blk);
    }

    alloc = ext2gd_alloc_slot(pos, type_sel);

    if (valid_bmp_slot(alloc)) {
        *gd_out = pos;
    }

done:
    ext2sb_unlock(sb);
    return alloc;
}

int
ext2ino_alloc_slot(struct v_superblock* vsb, struct ext2_gdesc** gd_out)
{
    return __ext2_global_slot_alloc(vsb, GDESC_INO_SEL, gd_out);
}

int
ext2db_alloc_slot(struct v_superblock* vsb, struct ext2_gdesc** gd_out)
{
    return __ext2_global_slot_alloc(vsb, GDESC_BLK_SEL, gd_out);
}

int
ext2gd_alloc_slot(struct ext2_gdesc* gd, int type_sel) 
{
    struct ext2_bmp* bmp;
    struct ext2_sbinfo *sb;
    int alloc;

    ext2gd_lock(gd);
    
    sb = gd->sb;
    bmp = &gd->bmps[type_sel];
    alloc = ext2bmp_alloc_nolock(bmp);
    
    if (alloc < 0) {
        goto done;
    }

    if (!ext2bmp_check_free(bmp)) {
        llist_delete(&gd->free_list_sel[type_sel]);
    }

    if (type_sel == GDESC_INO_SEL) {
        gd->info->bg_free_ino_cnt--;
        sb->raw->s_free_ino_cnt--;
    } else {
        gd->info->bg_free_blk_cnt--;
        sb->raw->s_free_blk_cnt--;
    }

    ext2gd_schedule_sync(gd);
    ext2sb_schedule_sync(sb);

done:
    ext2gd_unlock(gd);
    return alloc;
}

void
ext2gd_free_slot(struct ext2_gdesc* gd, int type_sel, int slot)
{
    struct llist_header *free_ent, *free_list;
    struct ext2_sbinfo *sb;

    ext2gd_lock(gd);

    ext2bmp_free_nolock(&gd->bmps[type_sel], slot);

    sb = gd->sb;
    free_ent  = &gd->free_list_sel[slot];
    free_list = &gd->sb->free_list_sel[slot];
    if (llist_empty(free_ent)) {
        llist_append(free_list, free_ent);
    }

    // FIXME might need arch-depedent impl for atomic operations
    if (type_sel == GDESC_INO_SEL) {
        gd->info->bg_free_ino_cnt++;
        sb->raw->s_free_ino_cnt++;
    } else {
        gd->info->bg_free_blk_cnt++;
        sb->raw->s_free_blk_cnt++;
    }

    ext2gd_schedule_sync(gd);
    ext2sb_schedule_sync(sb);

    ext2gd_unlock(gd);
}