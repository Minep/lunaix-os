#include <klibc/string.h>
#include <lunaix/ds/btrie.h>
#include <lunaix/fs.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>

#define PCACHE_DIRTY 0x1

void
pcache_init(struct pcache* pcache)
{
    btrie_init(&pcache->tree, PG_SIZE_BITS);
    llist_init_head(&pcache->dirty);
    llist_init_head(&pcache->pages);
}

void
pcache_release_page(struct pcache* pcache, struct pcache_pg* page)
{
    pmm_free_page(KERNEL_PID, page->pg);
    vmm_del_mapping(PD_REFERENCED, page->pg);

    llist_delete(&page->pg_list);

    vfree(page);

    pcache->n_pages--;
}

struct pcache_pg*
pcache_new_page(struct pcache* pcache, uint32_t index)
{
    void* pg = pmm_alloc_page(KERNEL_PID, 0);
    void* pg_v = vmm_vmap(pg, PG_SIZE, PG_PREM_URW);
    struct pcache_pg* ppg = vzalloc(sizeof(struct pcache_pg));
    ppg->pg = pg_v;

    llist_append(&pcache->pages, &ppg->pg_list);
    btrie_set(&pcache->tree, index, ppg);

    return ppg;
}

void
pcache_set_dirty(struct pcache* pcache, struct pcache_pg* pg)
{
    if (!(pg->flags & PCACHE_DIRTY)) {
        pg->flags |= PCACHE_DIRTY;
        pcache->n_dirty++;
        llist_append(&pcache->dirty, &pg->dirty_list);
    }
}

struct pcache_pg*
pcache_get_page(struct pcache* pcache,
                uint32_t index,
                uint32_t* offset,
                struct pcache_pg** page)
{
    struct pcache_pg* pg = btrie_get(&pcache->tree, index);
    int is_new = 0;
    *offset = index & ((1 << pcache->tree.truncated) - 1);
    if (!pg) {
        pg = pcache_new_page(pcache, index);
        pg->fpos = index - *offset;
        pcache->n_pages++;
        is_new = 1;
    }
    *page = pg;
    return is_new;
}

int
pcache_write(struct v_file* file, void* data, uint32_t len, uint32_t fpos)
{
    uint32_t pg_off, buf_off = 0;
    struct pcache* pcache = file->inode->pg_cache;
    struct pcache_pg* pg;

    while (buf_off < len) {
        pcache_get_page(pcache, fpos, &pg_off, &pg);
        uint32_t wr_bytes = MIN(PG_SIZE - pg_off, len - buf_off);
        memcpy(pg->pg + pg_off, (data + buf_off), wr_bytes);

        pcache_set_dirty(pcache, pg);

        buf_off += wr_bytes;
        fpos += wr_bytes;
    }

    return buf_off;
}

int
pcache_read(struct v_file* file, void* data, uint32_t len, uint32_t fpos)
{
    uint32_t pg_off, buf_off = 0, new_pg = 0;
    int errno = 0;
    struct pcache* pcache = file->inode->pg_cache;
    struct pcache_pg* pg;
    struct v_inode* inode = file->inode;

    while (buf_off < len) {
        if (pcache_get_page(pcache, fpos, &pg_off, &pg)) {
            // Filling up the page
            errno = inode->default_fops.read(file, pg->pg, PG_SIZE, pg->fpos);
            if (errno >= 0 && errno < PG_SIZE) {
                // EOF
                len = buf_off + errno;
            } else if (errno < 0) {
                break;
            }
        }
        uint32_t rd_bytes = MIN(PG_SIZE - pg_off, len - buf_off);
        memcpy((data + buf_off), pg->pg + pg_off, rd_bytes);

        buf_off += rd_bytes;
        fpos += rd_bytes;
    }

    return errno < 0 ? errno : buf_off;
}

void
pcache_release(struct pcache* pcache)
{
    struct pcache_pg *pos, *n;
    llist_for_each(pos, n, &pcache->pages, pg_list)
    {
        vfree(pos);
    }

    btrie_release(&pcache->tree);
}

int
pcache_commit(struct v_file* file, struct pcache_pg* page)
{
    if (!(page->flags & PCACHE_DIRTY)) {
        return;
    }

    struct v_inode* inode = file->inode;
    int errno = inode->default_fops.write(file, page->pg, PG_SIZE, page->fpos);

    if (!errno) {
        page->flags &= ~PCACHE_DIRTY;
        llist_delete(&page->dirty_list);
        file->inode->pg_cache->n_dirty--;
    }

    return errno;
}

void
pcache_commit_all(struct v_file* file)
{
    struct pcache* cache = file->inode->pg_cache;
    struct pcache_pg *pos, *n;
    llist_for_each(pos, n, &cache->dirty, dirty_list)
    {
        pcache_commit(file, pos);
    }
}

void
pcache_invalidate(struct v_file* file, struct pcache_pg* page)
{
    pcache_commit(file, page);
    pcache_release_page(&file->inode->pg_cache, page);
}