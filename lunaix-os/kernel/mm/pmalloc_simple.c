#include <lunaix/spike.h>
#include "pmm_internal.h"

#ifdef CONFIG_PMALLOC_SIMPLE

// Simple PM Allocator (segregated next fit)

#define INIT_FLAG   0b10000000
#define PO_FLAGS    0b01111111

static const int po_limit[] = {
    CONFIG_PMALLOC_SIMPLE_PO0_THRES,
    CONFIG_PMALLOC_SIMPLE_PO1_THRES,
    CONFIG_PMALLOC_SIMPLE_PO2_THRES,
    CONFIG_PMALLOC_SIMPLE_PO3_THRES,
    CONFIG_PMALLOC_SIMPLE_PO4_THRES,
    CONFIG_PMALLOC_SIMPLE_PO5_THRES,
    CONFIG_PMALLOC_SIMPLE_PO6_THRES,
    CONFIG_PMALLOC_SIMPLE_PO7_THRES,
    CONFIG_PMALLOC_SIMPLE_PO8_THRES,
    CONFIG_PMALLOC_SIMPLE_PO9_THRES,
};

static inline bool
__uninitialized_page(struct ppage* page)
{
    return !(page->flags & INIT_FLAG);
}

static inline void
__set_page_initialized(struct ppage* page)
{
    page->flags |= INIT_FLAG;
}

static inline void
__set_page_uninitialized(struct ppage* page)
{
    page->flags &= ~INIT_FLAG;
}

static inline void
__set_page_order(struct ppage* page, int order)
{
    page->flags = page->flags & ~PO_FLAGS | order & PO_FLAGS;
}

static inline int
__page_order(struct ppage* page)
{
    return page->flags & PO_FLAGS;
}

void
pmm_allocator_init(struct pmem* memory)
{
    // nothing todo

}

void
pmm_free_one(struct ppage* page, int type_mask)
{
    page = leading_page(page);
    struct pmem_pool* pool = pmm_pool_lookup(page);

    assert(page->refs);

    if (--page->refs) {
        return;
    }

    int order = __page_order(page);
    assert(order <= MAX_PAGE_ORDERS);

    struct llist_header* bucket = &pool->idle_order[order];

    if (pool->count[order] < po_limit[order]) {
        llist_append(bucket, &page->sibs);
        pool->count[order]++;
        return;
    }

    for (int i = 0; i < (1 << order); i++) {
        __set_page_uninitialized(&page[i]);
    }
}

static pfn_t index = 0;

struct ppage*
pmm_looknext(struct pmem_pool* pool, size_t order)
{
    struct ppage* tail, *lead;
    pfn_t working = index + 1;
    size_t count, total;
    size_t poolsz = ppfn_of(pool, pool->pool_end) + 1;

    total = 1 << order;
    count = total;
    while (!count && working != index)
    {
        tail = ppage_of(pool, working);

        // we use `head` to indicate an uninitialized page
        if (!__uninitialized_page(tail)) {
            count--;
        }
        else {
            count = total;
        }

        working = (working + 1) % poolsz;
    }

    index = working - 1;
    if (count) {
        return NULL;
    }

    lead = tail - total;
    for (int i = 0; i < total; i++)
    {
        struct ppage* page = &lead[i];
        page->companion = i;
        page->pool = pool->type;
        llist_init_head(&page->sibs);
    }
}

struct ppage*
pmm_alloc_napot_type(int pool, size_t order, pp_attr_t type)
{
    assert(order <= MAX_PAGE_ORDERS);

    struct pmem_pool* _pool = pmm_pool_get(pool);
    struct llist_header* bucket = &_pool->idle_order[order];

    struct ppage* good_page = NULL;
    if (!llist_empty(bucket)) {
        (_pool->count[order])--;
        good_page = list_entry(bucket->next, struct ppage, sibs);
        llist_delete(good_page);
    }
    else {
        good_page = pmm_looknext(_pool, order);
    }

    assert(good_page);
    good_page->refs = 1;
    good_page->type = type;

    return good_page;
}

struct ppage*
leading_page(struct ppage* page)
{   
    return __ppage(page - page->companion);
}

void
pmm_allocator_add_freehole(struct pmem_pool* pool, struct ppage* start, struct ppage* end)
{
    for (; start <= end; start++) {
        *start = (struct ppage){ };
    }
}

#endif