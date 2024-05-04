#include <klibc/string.h>
#include <lunaix/ds/btrie.h>
#include <lunaix/fs.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/bcache.h>

#define pcache_obj(bcache) container_of(bcache, struct pcache, cache)

void pcache_release_page(struct pcache* pcache, struct pcache_pg* page);
void pcache_set_dirty(struct pcache* pcache, struct pcache_pg* pg);

static bcache_zone_t pagecached_zone = NULL;

static void
__pcache_sync(struct bcache* bc, unsigned long tag, void* data)
{
    struct pcache* cache;

    cache = pcache_obj(bc);
    pcache_commit(cache->master, (struct pcache_pg*)data);
}

static void
__pcache_try_release(struct bcache* bc, void* data)
{
    struct pcache_pg* page;
    
    page = (struct pcache_pg*)data;
    pcache_release_page(pcache_obj(bc), page);
}

static struct bcache_ops cache_ops = {
    .release_on_evict = __pcache_try_release,
    .sync_cached = __pcache_sync
};

static void*
pcache_alloc_page()
{
    int i = 0;
    ptr_t va = 0;
    struct leaflet* leaflet = alloc_leaflet(0);

    if (!leaflet) {
        return NULL;
    }

    if (!(va = (ptr_t)vmap(leaflet, KERNEL_DATA))) {
        leaflet_return(leaflet);
        return NULL;
    }

    return (void*)va;
}

static void
pcache_free_page(void* va)
{
    pte_t* ptep = mkptep_va(VMS_SELF, (ptr_t)va);
    pte_t pte = pte_at(ptep);
    leaflet_return(pte_leaflet(pte));
}

void
pcache_init(struct pcache* pcache)
{
    if (unlikely(!pagecached_zone)) {
        pagecached_zone = bcache_create_zone("pcache");
    }

    llist_init_head(&pcache->dirty);

    bcache_init_zone(&pcache->cache, pagecached_zone, 4, -1, 
                     sizeof(struct pcache_pg), &cache_ops);
}

void
pcache_release_page(struct pcache* pcache, struct pcache_pg* page)
{
    pcache_free_page(page->data);

    vfree(page);

    pcache->n_pages--;
}

struct pcache_pg*
pcache_new_page(struct pcache* pcache)
{
    struct pcache_pg* ppg;
    void* data_page;
    
    data_page = pcache_alloc_page();
    if (!data_page) {
        return NULL;
    }

    ppg = vzalloc(sizeof(struct pcache_pg));
    ppg->data = data_page;

    return ppg;
}

void
pcache_set_dirty(struct pcache* pcache, struct pcache_pg* pg)
{
    if (pg->dirty) {
        return;
    }

    pg->dirty = true;
    pcache->n_dirty++;
    llist_append(&pcache->dirty, &pg->dirty_list);
}

static bcobj_t
__getpage_and_lock(struct pcache* pcache, unsigned int tag, 
                   struct pcache_pg** page)
{
    bcobj_t cobj;
    struct pcache_pg* pg;

    if (bcache_tryget(&pcache->cache, tag, &cobj))
    {
        *page = (struct pcache_pg*)bcached_data(cobj);
        return cobj;
    }

    pg = pcache_new_page(pcache);
    if (pg) {
        pg->index = tag;
    }

    *page = pg;

    return NULL;
}

static inline int
__fill_page(struct v_inode* inode, struct pcache_pg* pg, unsigned int index)
{
    return inode->default_fops->read_page(inode, pg->data, page_addr(index));
}

int
pcache_write(struct v_inode* inode, void* data, u32_t len, u32_t fpos)
{
    int errno = 0;
    unsigned int tag, off, wr_cnt;
    unsigned int end = fpos + len;
    struct pcache* pcache;
    struct pcache_pg* pg;
    bcobj_t obj;

    pcache = inode->pg_cache;

    while (fpos < page_upaligned(end) && errno >= 0) {
        tag = pfn(fpos);
        off = va_offset(fpos);
        wr_cnt = MIN(end - fpos, PAGE_SIZE - off);

        obj = __getpage_and_lock(pcache, tag, &pg);

        if (!obj && !pg) {
            errno = inode->default_fops->write(inode, data, fpos, wr_cnt);
            goto cont;
        }

        // new page and unaligned write, then prepare for partial override
        if (!obj && wr_cnt != PAGE_SIZE) {
            errno = __fill_page(inode, pg, tag);
            if (errno < 0) {
                return errno;
            }
        }

        memcpy(pg->data, data, wr_cnt);
        pcache_set_dirty(pcache, pg);

        if (obj) {
            bcache_return(obj);
        } else {
            bcache_put(&pcache->cache, tag, pg);
        }

cont:
        data += wr_cnt;
        fpos = page_aligned(fpos + PAGE_SIZE);
    }

    return errno < 0 ? errno : (int)((fpos + len) - end);
}

int
pcache_read(struct v_inode* inode, void* data, u32_t len, u32_t fpos)
{
    int errno = 0;
    unsigned int tag, off, rd_cnt;
    unsigned int end = fpos + len;
    struct pcache* pcache;
    struct pcache_pg* pg;
    bcobj_t obj;

    pcache = inode->pg_cache;

    while (fpos < page_upaligned(end)) {
        tag = pfn(fpos);
        off = va_offset(fpos);
        rd_cnt = MIN(end - fpos, PAGE_SIZE - off);

        obj = __getpage_and_lock(pcache, tag, &pg);

        if (!obj) {
            errno = __fill_page(inode, pg, tag);
            if (errno < 0) {
                return errno;
            }

            end -= (PAGE_SIZE - errno);
        }

        memcpy(data, pg->data + off, rd_cnt);

        if (obj) {
            bcache_return(obj);
        } else {
            bcache_put(&pcache->cache, tag, pg);
        }

        data += rd_cnt;
        fpos = page_aligned(fpos + PAGE_SIZE);
    }

    return errno < 0 ? errno : (int)((fpos + len) - end);
}

void
pcache_release(struct pcache* pcache)
{
    bcache_free(&pcache->cache);
}

int
pcache_commit(struct v_inode* inode, struct pcache_pg* page)
{
    if (!page->dirty) {
        return 0;
    }

    int errno;
    unsigned int fpos = page_addr(page->index);
    
    errno = inode->default_fops->write_page(inode, page->data, fpos);
    if (!errno) {
        page->dirty = false;
        llist_delete(&page->dirty_list);
        inode->pg_cache->n_dirty--;
    }

    return errno;
}

void
pcache_commit_all(struct v_inode* inode)
{
    struct pcache* cache = inode->pg_cache;
    if (!cache) {
        return;
    }

    struct pcache_pg *pos, *n;
    llist_for_each(pos, n, &cache->dirty, dirty_list)
    {
        pcache_commit(inode, pos);
    }
}