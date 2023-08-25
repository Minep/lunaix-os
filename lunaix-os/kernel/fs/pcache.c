#include <klibc/string.h>
#include <lunaix/ds/btrie.h>
#include <lunaix/fs.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>

#define PCACHE_DIRTY 0x1

static struct lru_zone* pcache_zone;

static int
__pcache_try_evict(struct lru_node* obj)
{
    struct pcache_pg* page = container_of(obj, struct pcache_pg, lru);
    pcache_invalidate(page->holder, page);
    return 1;
}

static void
pcache_free_page(void* va)
{
    ptr_t pa = vmm_del_mapping(VMS_SELF, (ptr_t)va);
    pmm_free_page(KERNEL_PID, pa);
}

static void*
pcache_alloc_page()
{
    int i = 0;
    ptr_t pp = pmm_alloc_page(KERNEL_PID, 0), va = 0;

    if (!pp) {
        return NULL;
    }

    if (!(va = (ptr_t)vmap(pp, PG_SIZE, PG_PREM_RW, 0))) {
        pmm_free_page(KERNEL_PID, pp);
        return NULL;
    }

    return (void*)va;
}

void
pcache_init(struct pcache* pcache)
{
    btrie_init(&pcache->tree, PG_SIZE_BITS);
    llist_init_head(&pcache->dirty);
    llist_init_head(&pcache->pages);

    pcache_zone = lru_new_zone(__pcache_try_evict);
}

void
pcache_release_page(struct pcache* pcache, struct pcache_pg* page)
{
    pcache_free_page(page->pg);

    llist_delete(&page->pg_list);

    btrie_remove(&pcache->tree, page->fpos);

    vfree(page);

    pcache->n_pages--;
}

struct pcache_pg*
pcache_new_page(struct pcache* pcache, u32_t index)
{
    struct pcache_pg* ppg = vzalloc(sizeof(struct pcache_pg));
    void* pg = pcache_alloc_page();

    if (!ppg || !pg) {
        lru_evict_one(pcache_zone);
        if (!ppg && !(ppg = vzalloc(sizeof(struct pcache_pg)))) {
            return NULL;
        }

        if (!pg && !(pg = pcache_alloc_page())) {
            return NULL;
        }
    }

    ppg->pg = pg;
    ppg->holder = pcache;

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

int
pcache_get_page(struct pcache* pcache,
                u32_t index,
                u32_t* offset,
                struct pcache_pg** page)
{
    struct pcache_pg* pg = btrie_get(&pcache->tree, index);
    int is_new = 0;
    u32_t mask = ((1 << pcache->tree.truncated) - 1);
    *offset = index & mask;
    if (!pg && (pg = pcache_new_page(pcache, index))) {
        pg->fpos = index & ~mask;
        pcache->n_pages++;
        is_new = 1;
    }
    if (pg)
        lru_use_one(pcache_zone, &pg->lru);
    *page = pg;
    return is_new;
}

int
pcache_write(struct v_inode* inode, void* data, u32_t len, u32_t fpos)
{
    int errno = 0;
    u32_t pg_off, buf_off = 0;
    struct pcache* pcache = inode->pg_cache;
    struct pcache_pg* pg;

    while (buf_off < len) {
        u32_t wr_bytes = MIN(PG_SIZE - pg_off, len - buf_off);

        pcache_get_page(pcache, fpos, &pg_off, &pg);

        if (!pg) {
            errno = inode->default_fops->write(inode, data, wr_bytes, fpos);
            if (errno < 0) {
                break;
            }
        } else {
            memcpy(pg->pg + pg_off, (data + buf_off), wr_bytes);
            pcache_set_dirty(pcache, pg);

            pg->len = pg_off + wr_bytes;
        }

        buf_off += wr_bytes;
        fpos += wr_bytes;
    }

    return errno < 0 ? errno : (int)buf_off;
}

int
pcache_read(struct v_inode* inode, void* data, u32_t len, u32_t fpos)
{
    u32_t pg_off, buf_off = 0, new_pg = 0;
    int errno = 0;
    struct pcache* pcache = inode->pg_cache;
    struct pcache_pg* pg;

    while (buf_off < len) {
        int new_page = pcache_get_page(pcache, fpos, &pg_off, &pg);
        if (new_page) {
            // Filling up the page
            errno =
              inode->default_fops->read_page(inode, pg->pg, PG_SIZE, pg->fpos);

            if (errno < 0) {
                break;
            }
            if (errno < PG_SIZE) {
                // EOF
                len = MIN(len, buf_off + errno);
            }

            pg->len = errno;
        } else if (!pg) {
            errno = inode->default_fops->read_page(
              inode, (data + buf_off), len - buf_off, pg->fpos);
            buf_off = len;
            break;
        }

        u32_t rd_bytes = MIN(pg->len - pg_off, len - buf_off);

        if (!rd_bytes)
            break;

        memcpy((data + buf_off), pg->pg + pg_off, rd_bytes);

        buf_off += rd_bytes;
        fpos += rd_bytes;
    }

    return errno < 0 ? errno : (int)buf_off;
}

void
pcache_release(struct pcache* pcache)
{
    struct pcache_pg *pos, *n;
    llist_for_each(pos, n, &pcache->pages, pg_list)
    {
        lru_remove(pcache_zone, &pos->lru);
        vfree(pos);
    }

    btrie_release(&pcache->tree);
}

int
pcache_commit(struct v_inode* inode, struct pcache_pg* page)
{
    if (!(page->flags & PCACHE_DIRTY)) {
        return 0;
    }

    int errno =
      inode->default_fops->write_page(inode, page->pg, PG_SIZE, page->fpos);

    if (!errno) {
        page->flags &= ~PCACHE_DIRTY;
        llist_delete(&page->dirty_list);
        inode->pg_cache->n_dirty--;
    }

    return errno;
}

void
pcache_commit_all(struct v_inode* inode)
{
    if (!inode->pg_cache) {
        return;
    }

    struct pcache* cache = inode->pg_cache;
    struct pcache_pg *pos, *n;

    llist_for_each(pos, n, &cache->dirty, dirty_list)
    {
        pcache_commit(inode, pos);
    }
}

void
pcache_invalidate(struct pcache* pcache, struct pcache_pg* page)
{
    pcache_commit(pcache->master, page);
    pcache_release_page(pcache, page);
}