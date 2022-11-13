#include <lunaix/dirent.h>
#include <lunaix/fs.h>
#include <lunaix/fs/iso9660.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

#include <klibc/string.h>

extern struct cake_pile* drec_cache_pile;

void
iso9660_fill_drecache(struct iso_drecache* cache,
                      struct iso_drecord* drec,
                      u32_t drec_len)
{
    *cache = (struct iso_drecache){ .data_size = drec->data_size.le,
                                    .extent_addr = drec->extent_addr.le,
                                    .flags = drec->flags,
                                    .fu_size = drec->fu_sz ? drec->fu_sz : 1,
                                    .gap_size = drec->gap_sz,
                                    .xattr_len = drec->xattr_len };
    u32_t padding = ((drec->name.len + sizeof(drec->name)) % 2) != 0;
    u32_t su_offset = drec->name.len + sizeof(struct iso_drecord) + padding;
    int su_len = drec_len - su_offset - 2, i = 0;

    while (i < su_len) {
        struct isosu_base* su_entry =
          (struct isosu_base*)((void*)drec + su_offset + i);
        switch (su_entry->signature) {
            case ISORR_NM:
                i += isorr_parse_nm(cache, (void*)su_entry);
                break;
            case ISORR_PX:
                i += isorr_parse_px(cache, (void*)su_entry);
                break;
            case ISORR_TF:
                i += isorr_parse_tf(cache, (void*)su_entry);
                break;
            case ISOSU_ST:
                goto done;
            default:
                i += su_entry->length;
                break;
        }
    }

done:
    if (!cache->name.len) {
        // Load ISO9660 file id if no NM found.
        u32_t l = drec->name.len;
        while (l < (u32_t)-1 && drec->name.content[l--] != ';')
            ;
        l = (l + 1) ? l : drec->name.len;
        l = MIN(l, ISO9660_IDLEN - 1);

        strncpy(cache->name_val, drec->name.content, l);
        cache->name = HSTR(cache->name_val, l);
        hstr_rehash(&cache->name, HSTR_FULL_HASH);
    }
}

int
iso9660_setup_dnode(struct v_dnode* dnode, struct v_inode* inode)
{
    if (!(inode->itype & VFS_IFDIR)) {
        vfs_assign_inode(dnode, inode);
        return 0;
    }

    struct iso_inode* isoino = inode->data;
    if (!llist_empty(&isoino->drecaches)) {
        dnode->data = &isoino->drecaches;
        vfs_assign_inode(dnode, inode);
        return 0;
    }

    int errno = 0;
    struct device* dev = dnode->super_block->dev;
    void* records = valloc(ISO9660_BLKSZ);
    u32_t current_pos = -ISO9660_BLKSZ, max_pos = inode->fsize,
          blk = inode->lb_addr * ISO9660_BLKSZ, blk_offset = (u32_t)-1;

    // As per 6.8.1, Directory structure shall NOT recorded in interleave mode.
    do {
        if (blk_offset >= ISO9660_BLKSZ - sizeof(struct iso_drecord)) {
            current_pos += ISO9660_BLKSZ;
            errno = dev->read(dev, records, blk + current_pos, ISO9660_BLKSZ);
            if (errno < 0) {
                errno = EIO;
                goto done;
            }
            blk_offset = 0;
        }

        struct iso_drecord* drec;
        struct iso_var_mdu* mdu = (struct iso_var_mdu*)(records + blk_offset);

        if (!(drec = iso9660_get_drecord(mdu))) {
            break;
        }

        // ignore the '.', '..' as we have built-in support
        if (drec->name.len == 1) {
            goto cont;
        }

        struct iso_drecache* cache = cake_grab(drec_cache_pile);

        iso9660_fill_drecache(cache, drec, mdu->len);
        llist_append(&isoino->drecaches, &cache->caches);
    cont:
        blk_offset += mdu->len;
    } while (current_pos + blk_offset < max_pos);

    dnode->data = &isoino->drecaches;

    vfs_assign_inode(dnode, inode);

    errno = 0;

done:
    vfree(records);
    return errno;
}

int
iso9660_dir_lookup(struct v_inode* this, struct v_dnode* dnode)
{
    struct iso_inode* isoino = this->data;
    struct iso_drecache *pos, *n;

    llist_for_each(pos, n, &isoino->drecaches, caches)
    {
        if (HSTR_EQ(&dnode->name, &pos->name)) {
            goto found;
        }
    }

    return ENOENT;
found:
    struct v_inode* inode = vfs_i_find(dnode->super_block, pos->extent_addr);

    if (!inode) {
        inode = vfs_i_alloc(dnode->super_block);
        iso9660_fill_inode(inode, pos, pos->extent_addr);
        vfs_i_addhash(inode);
    }

    return iso9660_setup_dnode(dnode, inode);
}

static int
__get_dtype(struct iso_drecache* pos)
{
    if ((pos->flags & ISO_FDIR)) {
        return DT_DIR;
    } else {
        return DT_FILE;
    }
}

int
iso9660_readdir(struct v_file* file, struct dir_context* dctx)
{
    struct llist_header* lead = file->dnode->data;
    struct iso_drecache *pos, *n;
    u32_t counter = dctx->index - 1;

    llist_for_each(pos, n, lead, caches)
    {
        if (counter == (u32_t)-1 && !(pos->flags & ISO_FHIDDEN)) {
            dctx->read_complete_callback(
              dctx, pos->name_val, pos->name.len, __get_dtype(pos));
            return 1;
        }
        counter--;
    }
    return 0;
}