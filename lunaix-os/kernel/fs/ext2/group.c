#include <lunaix/mm/valloc.h>

#include "ext2.h"

bcache_zone_t gdesc_bcache_zone;

static void
__cached_gdesc_evict(struct bcache* bc, void* data)
{
    struct ext2_gdesc* gd;
    gd = (struct ext2_gdesc*)data;

    llist_delete(&gd->groups);
    llist_delete(&gd->free_grps_blk);
    llist_delete(&gd->free_grps_ino);

    fsblock_put(gd->buf);

    vfree(gd);
}

static void
__cached_gdesc_sync(struct bcache*, unsigned long tag, void* data)
{
    // since all mods to gdesc goes directly into fs buffer,
    // we just need to invoke the sync on the underlying.

    struct ext2_gdesc* gd;
    gd = (struct ext2_gdesc*)data;

    fsblock_sync(gd->buf);
}

static struct bcache_ops gdesc_bc_ops = {
    .release_on_evict = __cached_gdesc_evict,
    .sync_cached = __cached_gdesc_sync
};

void
ext2gd_prepare_gdt(struct v_superblock* vsb)
{
    struct ext2b_super* sb;
    unsigned int nr_parts;
    unsigned int nr_gd_pb, nr_gd;
    struct ext2_sbinfo* ext2sb;

    ext2sb = EXT2_SB(vsb);
    sb = ext2sb->raw;
    
    nr_gd_pb = ext2sb->block_size / sizeof(struct ext2b_gdesc);
    nr_gd    = ICEIL(sb->s_blk_cnt, sb->s_blk_per_grp);
    nr_parts = ICEIL(nr_gd, nr_gd_pb);

    ext2sb->gdt_frag    = (bbuf_t*)vcalloc(sizeof(bbuf_t), nr_parts);
    ext2sb->nr_gdesc_pb = nr_gd_pb;
    ext2sb->nr_gdesc    = nr_gd;

    bcache_init_zone(&ext2sb->gd_caches, gdesc_bcache_zone, 
                ilog2(64), 0, sizeof(struct ext2b_gdesc), &gdesc_bc_ops);

    llist_init_head(&ext2sb->gds);
    llist_init_head(&ext2sb->free_grps_blk);
    llist_init_head(&ext2sb->free_grps_ino);
}

void
ext2gd_release_gdt(struct v_superblock* vsb)
{
    unsigned int parts_cnt;
    struct ext2_sbinfo* ext2sb;

    ext2sb = EXT2_SB(vsb);
    parts_cnt = ICEIL(ext2sb->nr_gdesc, ext2sb->nr_gdesc_pb);
    for (size_t i = 0; i < parts_cnt; i++)
    {
        if (!ext2sb->gdt_frag[i]) {
            continue;
        }

        fsblock_put(ext2sb->gdt_frag[i]);
        ext2sb->gdt_frag[i] = NULL;
    }
}

static inline bool
__try_load_bitmap(struct v_superblock* vsb, 
                  struct ext2_gdesc* gd, int type)
{
    struct ext2_sbinfo* ext2sb;
    struct ext2_bmp* bmp;
    struct llist_header* flist, *flist_entry;
    bbuf_t buf;
    unsigned int blk_id, bmp_blk_id, bmp_size;

    ext2sb = EXT2_SB(vsb);

    if (type == GDESC_INO_SEL) {
        bmp_blk_id = gd->info->bg_ino_map;
        bmp_size = ext2sb->raw->s_ino_per_grp;
        bmp = &gd->ino_bmp;
    }
    else if (type == GDESC_BLK_SEL) {
        bmp_blk_id = gd->info->bg_blk_map;
        bmp_size = ext2sb->raw->s_blk_per_grp;
        bmp = &gd->blk_bmp;
    }
    else {
        fail_fs("unknown bitmap type");
    }

    flist = &ext2sb->free_list_sel[type];
    flist_entry = &gd->free_list_sel[type];

    blk_id = ext2_datablock(vsb, bmp_blk_id);
    buf    = fsblock_get(vsb, blk_id);
    if (blkbuf_errbuf(buf)) {
        return false;
    }

    ext2bmp_init(bmp, buf, bmp_size);

    if (ext2bmp_check_free(bmp)) {
        llist_append(flist, flist_entry);
    }

    return true;
}

int
ext2gd_take(struct v_superblock* vsb, 
               unsigned int index, struct ext2_gdesc** out)
{
    bbuf_t part, buf;
    struct ext2_sbinfo* ext2sb;
    unsigned int blk_id, blk_off;

    ext2sb = EXT2_SB(vsb);

    if (index >= ext2sb->nr_gdesc) {
        return ENOENT;
    }
    
    bcobj_t cached;
    if (bcache_tryget(&ext2sb->gd_caches, index, &cached)) {
        *out = (struct ext2_gdesc*)bcached_data(cached);
        return 0;
    }

    blk_id  = index / ext2sb->nr_gdesc_pb;
    blk_off = index % ext2sb->nr_gdesc_pb;
    
    part = ext2sb->gdt_frag[blk_id];
    if (!part) {
        blk_id = ext2_datablock(vsb, blk_id + 1);
        part   = fsblock_get(vsb, blk_id);
        if (!part) {
            return EIO;
        }
        
        ext2sb->gdt_frag[blk_id] = part;
    }

    struct ext2_gdesc* gd;
    
    gd = valloc(sizeof(struct ext2_gdesc));
    *gd = (struct ext2_gdesc) {
        .info = &block_buffer(part, struct ext2b_gdesc)[blk_off],
        .buf  = part,
        .base = index * ext2sb->raw->s_blk_per_grp,
        .ino_base = index * ext2sb->raw->s_ino_per_grp
    };

    *out = gd;
    
    if (!ext2sb->read_only) {
        if (!__try_load_bitmap(vsb, gd, GDESC_INO_SEL)) {
            goto cleanup;
        }

        if (!__try_load_bitmap(vsb, gd, GDESC_BLK_SEL)) {
            llist_delete(&gd->free_grps_ino);
            goto cleanup;
        }
    }
    
    llist_append(&ext2sb->gds, &gd->groups);

    cached = bcache_put_and_ref(&ext2sb->gd_caches, index, gd);
    gd->cache_ref = cached;
    gd->sb = ext2sb;

    return 0;

cleanup:
    *out = NULL;

    vfree(gd);
    return EIO;
}

static void
__ext2bmp_update_next_free_cell(struct ext2_bmp* e_bmp)
{
    unsigned int i;
    unsigned int end;

    i = valid_bmp_slot(e_bmp->next_free) ? e_bmp->next_free : 0;
    end = i;
    
    // next fit, try to maximize our locality without going after
    //  some crazy algorithm
    do {
        if (e_bmp->bmp[i] != 0xff) {
            e_bmp->next_free = i;
            return;
        }

        if (++i == e_bmp->nr_bytes) {
            i = 0;
        }
    } 
    while (i != end);

    e_bmp->next_free = ALLOC_FAIL;
}

void
ext2bmp_init(struct ext2_bmp* e_bmp, bbuf_t bmp_buf, unsigned int nr_bits)
{
    assert(nr_bits % 8 == 0);

    e_bmp->bmp = blkbuf_data(bmp_buf);
    e_bmp->raw = bmp_buf;
    e_bmp->nr_bytes = nr_bits / 8;
    
    __ext2bmp_update_next_free_cell(e_bmp);
}

bool
ext2bmp_check_free(struct ext2_bmp* e_bmp)
{
    assert(e_bmp->raw);

    return valid_bmp_slot(e_bmp->next_free);
}

int
ext2bmp_alloc_one(struct ext2_bmp* e_bmp)
{
    assert(e_bmp->raw);
    
    u8_t cell;
    int slot, next_free;

    if (!valid_bmp_slot(e_bmp->next_free)) {
        return ALLOC_FAIL;
    }
    
    slot = 0;
    next_free = e_bmp->next_free;
    cell = e_bmp->bmp[next_free];
    assert(cell != 0xff);

    while ((cell & (1 << slot++)));

    cell |= (1 << --slot);
    slot += (next_free * 8);
    e_bmp->bmp[next_free] = cell;

    if (cell == 0xff) {
        __ext2bmp_update_next_free_cell(e_bmp);
    }

    fsblock_dirty(e_bmp->raw);
    return slot;
}

void
ext2bmp_free_one(struct ext2_bmp* e_bmp, unsigned int pos)
{
    assert(e_bmp->raw);

    int cell_idx = pos / 8;
    u8_t cell_mask = 1 << (pos % 8);
    e_bmp->bmp[cell_idx] &= ~cell_mask;

    if (!valid_bmp_slot(e_bmp->next_free)) {
        e_bmp->next_free = cell_idx;
    }

    fsblock_dirty(e_bmp->raw);
}

void
ext2bmp_discard(struct ext2_bmp* e_bmp)
{
    assert(e_bmp->raw);

    fsblock_put(e_bmp->raw);
    e_bmp->raw = NULL;
}