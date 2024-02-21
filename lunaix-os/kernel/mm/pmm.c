#include <lunaix/status.h>
#include <lunaix/mm/pagetable.h>
#include <lunaix/spike.h>

#include "pmm_internal.h"

static inline bool
__check_typemask(struct ppage* page, pp_attr_t typemask)
{
    return !page->type || (page->type & typemask);
}

static struct pmem memory;
export_symbol(debug, pmm, memory);

void
pmm_init_begin(struct boot_handoff* bctx)
{
    memory.state = PMM_IN_INIT;

    pmm_arch_init_begin(&memory, bctx);
    pmm_allocator_init(&memory);

    llist_init_head(&memory.reserved);
}

void
pmm_init_freeze_range(pfn_t start, size_t n) {
    assert(memory.state == PMM_IN_INIT);
    
    struct llist_header* rsvd = &memory.reserved;
    unsigned short companion_off;
    while (n) {
        companion_off = MIN(MAX_GROUP_PAGE_SIZE, n);

        // reserve
        struct ppage* page = ppage(start);
        for(size_t i = 0; i < companion_off; i++) {
            page[i] = (struct ppage) {
                .type = PP_RESERVED,
                .refs = RESERVE_MARKER,
                .companion = i  // point to lead page
            };
        }

        // add to reserved while keeping the order.
        struct ppage *pos, *nx;
        llist_for_each(pos, nx, rsvd, sibs) {
            if (&nx->sibs == rsvd) {
                break;
            }

            pfn_t p = __ppfn(page);
            if (__ppfn(pos) + pos->companion < p && p < __ppfn(nx)) {
                break;
            }
        }

        // we connect tails together, thus we can get back to lead page easily
        page += companion_off;
        llist_append(&pos->sibs, &page->sibs);

        n -= companion_off;
    }
}

void
pmm_init_end() {
    assert(memory.state == PMM_IN_INIT);
    memory.state = PMM_NORMAL;

    pmm_arch_init_pool(&memory);

    struct ppage *start, *end;
    start = memory.pplist;

    struct ppage *rsvd_tailpage, *n;
    llist_for_each(rsvd_tailpage, n, &memory.reserved, sibs)
    {

        struct pmem_pool* pool;
        for (int i = 0; i < POOL_COUNT || (pool = NULL); i++)
        {
            pool = &memory.pool[i];
            if (pool->pool_start <= __ppfn(start) 
                && __ppfn(start) <= pool->pool_end) 
            {
                end = (struct ppage*)MIN(pool->pool_end, rsvd_tailpage);
                break;
            }
        }
        
        assert(pool);

        pmm_allocator_add_freehole(pool, start, end);

        start = rsvd_tailpage + 1;
    }
    
}

bool
pmm_onhold_range(pfn_t start, size_t npages)
{
    
    struct ppage *_start, *_end, 
                 *_mark_start, *_mark_end;

    _start = ppage(start);
    _end = ppage(start + npages);
    
    struct pmem_pool* pool;
    for (int i = 0; i < POOL_COUNT; i++) {
        pool = &memory.pool[i];
        if (pool->pool_start > _start && _end > pool->pool_end) {
            continue;
        }

        _mark_start = MAX(pool->pool_start, _start);
        _mark_end   = MIN(pool->pool_end, _end);
        if (pmm_allocator_trymark_onhold(pool, _mark_start, _mark_end))
        {
            npages -= ((ptr_t)_mark_end - (ptr_t)_mark_start) + 1;
        }
    }

    return !npages;
}

struct pmem_pool*
pmm_pool_get(int pool_index)
{
    assert(pool_index < POOL_COUNT);

    return &memory.pool[pool_index];
}
