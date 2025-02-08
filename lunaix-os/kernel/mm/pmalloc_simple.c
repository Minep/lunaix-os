#include <lunaix/spike.h>
#include "pmm_internal.h"

#ifdef CONFIG_PMALLOC_METHOD_SIMPLE

// Simple PM Allocator (segregated next fit)

#define INIT_FLAG   0b10

static const int po_limit[] = {
    CONFIG_PMALLOC_SIMPLE_MAX_PO0,
    CONFIG_PMALLOC_SIMPLE_MAX_PO1,
    CONFIG_PMALLOC_SIMPLE_MAX_PO2,
    CONFIG_PMALLOC_SIMPLE_MAX_PO3,
    CONFIG_PMALLOC_SIMPLE_MAX_PO4,
    CONFIG_PMALLOC_SIMPLE_MAX_PO5,
    CONFIG_PMALLOC_SIMPLE_MAX_PO6,
    CONFIG_PMALLOC_SIMPLE_MAX_PO7,
    CONFIG_PMALLOC_SIMPLE_MAX_PO8,
    CONFIG_PMALLOC_SIMPLE_MAX_PO9,
};

static inline bool
__uninitialized_page(struct ppage* page)
{
    return !(page->flags & INIT_FLAG);
}

static inline void
__init_page(struct ppage* page)
{
    page->flags |= INIT_FLAG;
}

static inline void
__deinit_page(struct ppage* lead)
{
    for (size_t i = 0; i < (1UL << lead->order); i++)
    {
        lead[i].flags &= ~INIT_FLAG;
    }
}

void
pmm_allocator_init(struct pmem* memory)
{
    // nothing todo
}

void
pmm_allocator_init_pool(struct pmem_pool* pool)
{
    for (int i = 0; i < MAX_PAGE_ORDERS; i++) {
        list_head_init(&pool->idle_order[i]);
        pool->count[i] = 0;
    }

    struct ppage* pooled_page = pool->pool_start;
    for (; pooled_page <= pool->pool_end; pooled_page++) {
        *pooled_page = (struct ppage){ };
    }
}

void
pmm_free_one(struct ppage* page, int type_mask)
{
    page = leading_page(page);

    assert(page->refs);
    assert(!reserved_page(page));
    assert(!__uninitialized_page(page));

    if (--page->refs) {
        return;
    }

    int order = page->order;
    assert(order <= MAX_PAGE_ORDERS);

    struct pmem_pool* pool = pmm_pool_lookup(page);
    struct list_head* bucket = &pool->idle_order[order];

    if (pool->count[order] < po_limit[order]) {
        list_add(bucket, &page->sibs);
        pool->count[order] += 1;
        return;
    }

    __deinit_page(page);
}

static pfn_t index = 0;

struct ppage*
pmm_looknext(struct pmem_pool* pool, size_t order)
{
    struct ppage *lead, *tail = NULL;
    pfn_t working = index;
    size_t count, total;
    size_t poolsz = ppfn_of(pool, pool->pool_end) + 1;

    total = 1 << order;
    count = total;
    do
    {
        tail = ppage_of(pool, working);

        if (__uninitialized_page(tail)) {
            count--;
        }
        else {
            count = total;
        }

        working = (working + 1) % poolsz;
    } while (count && working != index);

    index = working;
    if (count) {
        return NULL;
    }

    lead = tail - total + 1;
    for (size_t i = 0; i < total; i++)
    {
        struct ppage* page = &lead[i];
        page->order = order;
        page->companion = i;
        page->pool = pool->type;
        page->refs = 0;
        list_node_init(&page->sibs);
        __init_page(page);
    }

    return lead;
}

static struct ppage*
__select_free_page_from(struct list_head* bucket)
{
    struct list_node* sib;
    struct ppage* page;

    do {
        sib = list_pop_head(bucket);
        page = sib ? slist_entry(sib, struct ppage, sibs) : NULL;
    } while (page && __uninitialized_page(page));

    return page;
}

struct ppage*
pmm_alloc_napot_type(int pool, size_t order, ppage_type_t type)
{
    assert(order <= MAX_PAGE_ORDERS);

    struct pmem_pool* _pool = pmm_pool_get(pool);
    struct list_head* bucket = &_pool->idle_order[order];
    struct ppage* good_page = NULL;

    if (!list_empty(bucket)) {
        _pool->count[order] -= 1;
        good_page = __select_free_page_from(bucket);
    }
    
    if (!good_page) {
        good_page = pmm_looknext(_pool, order);
    }

    assert(good_page);
    assert(!good_page->refs);
    
    good_page->refs = 1;
    good_page->type = type;

    return good_page;
}

bool
pmm_allocator_trymark_onhold(struct pmem_pool* pool, 
                             struct ppage* start, struct ppage* end)
{
    while (start <= end) {
        if (__uninitialized_page(start)) {
            set_reserved(start);
            __init_page(start);
        }
        else if (!start->refs) {
            __deinit_page(leading_page(start));
            continue;
        }
        else if (!reserved_page(start)) {
            return false;
        }

        start++;
    }

    return true;
}

bool
pmm_allocator_trymark_unhold(struct pmem_pool* pool, 
                             struct ppage* start, struct ppage* end)
{
    while (start <= end) {
        if (!__uninitialized_page(start) && reserved_page(start)) {
            __deinit_page(start);
        }

        start++;
    }

    return true;
}

#endif