#include <lunaix/spike.h>
#include "pmm_internal.h"

#ifdef CONFIG_PMALLOC_SIMPLE

// Simple PM Allocator (segregated next fit)

#define INIT_FLAG   0b10

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
__set_page_uninitialized(struct ppage* lead)
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
        llist_init_head(&pool->idle_order[i]);
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

    if (reserved_page(page) || --page->refs) {
        return;
    }

    int order = page->order;
    assert(order <= MAX_PAGE_ORDERS);

    struct pmem_pool* pool = pmm_pool_lookup(page);
    struct llist_header* bucket = &pool->idle_order[order];

    if (pool->count[order] < po_limit[order]) {
        llist_append(bucket, &page->sibs);
        pool->count[order]++;
        return;
    }

    __set_page_uninitialized(page);
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
        llist_init_head(&page->sibs);
        __set_page_initialized(page);
    }

    return lead;
}

struct ppage*
pmm_alloc_napot_type(int pool, size_t order, ppage_type_t type)
{
    assert(order <= MAX_PAGE_ORDERS);

    struct pmem_pool* _pool = pmm_pool_get(pool);
    struct llist_header* bucket = &_pool->idle_order[order];

    struct ppage* good_page = NULL;
    if (!llist_empty(bucket)) {
        (_pool->count[order])--;
        good_page = list_entry(bucket->next, struct ppage, sibs);
        llist_delete(&good_page->sibs);
    }
    else {
        good_page = pmm_looknext(_pool, order);
    }

    assert(good_page);
    good_page->refs = 1;
    good_page->type = type;

    return good_page;
}

bool
pmm_allocator_trymark_onhold(struct pmem_pool* pool, struct ppage* start, struct ppage* end)
{
    while (start <= end) {
        if (__uninitialized_page(start)) {
            set_reserved(start);
            __set_page_initialized(start);
        }
        else if (!start->refs) {
            struct ppage* lead = leading_page(start);
            llist_delete(&lead->sibs);

            __set_page_uninitialized(lead);
            
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
pmm_allocator_trymark_unhold(struct pmem_pool* pool, struct ppage* start, struct ppage* end)
{
    while (start <= end) {
        if (!__uninitialized_page(start) && reserved_page(start)) {
            __set_page_uninitialized(start);
        }

        start++;
    }

    return true;
}

#endif