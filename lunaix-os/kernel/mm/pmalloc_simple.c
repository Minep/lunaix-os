#include <lunaix/spike.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/pgpol.h>
#include <lunaix/mm/physical.h>
#include <lunaix/mm/pmalloc.h>

#ifdef CONFIG_PMALLOC_METHOD_SIMPLE

// Simple PM Allocator (segregated next fit)

#define NR_PAGE_ORDERS 5

struct pmalloc_simple {
    struct llist_header idle_order[NR_PAGE_ORDERS];
    int count[NR_PAGE_ORDERS];
    pfn_t index;
};

static const int po_limit[] = {
    CONFIG_PMALLOC_SIMPLE_MAX_PO0,
    CONFIG_PMALLOC_SIMPLE_MAX_PO1,
    CONFIG_PMALLOC_SIMPLE_MAX_PO2,
    CONFIG_PMALLOC_SIMPLE_MAX_PO3,
    CONFIG_PMALLOC_SIMPLE_MAX_PO4,
    CONFIG_PMALLOC_SIMPLE_MAX_PO5,
};

static inline struct pmalloc_simple*
__get_allocator(struct pmpool* pool) {
    return (struct pmalloc_simple*)pool->alloc_private;
}

static void
__free_one(struct pmpool* pool, struct ppage* page)
{
    struct pmalloc_simple* alloc;
    struct llist_header* bucket;
    struct ppage* pos;

    int order = page->order;
    assert(order <= NR_PAGE_ORDERS);
    
    alloc = __get_allocator(pool);
    bucket = &alloc->idle_order[order];

    foreach_page(page, pos) {
        pos->pol = __PGPOL_NONE;
    }

    if (alloc->count[order] < po_limit[order]) {
        llist_append(bucket, &page->sibs);
        alloc->count[order]++;
        return;
    }
}


static struct ppage*
__looknext(struct pmpool* pool, size_t order)
{
    struct pmalloc_simple* alloc;
    struct ppage *lead, *tail = NULL;
    pfn_t working;
    size_t count, total;
    size_t poolsz;

    alloc = __get_allocator(pool);
    poolsz = ppfn_of(pool, pool->last) + 1;
    working = alloc->index;

    total = 1 << order;
    count = total;

    do {
        tail = ppage_of(pool, working);

        if (!tail->pol) {
            count--;
        }
        else {
            count = total;
        }

        working = (working + 1) % poolsz;
    } while (count && working != alloc->index);

    alloc->index = working;

    if (count)
        return NULL;

    lead = tail - total + 1;
    for (size_t i = 0; i < total; i++)
    {
        struct ppage* page = &lead[i];

        page->order = order;
        page->companion = i;
        page->pool = pool->type;

        llist_init_head(&page->sibs);
    }

    return lead;
}

static struct ppage*
__alloc_napot_type(struct pmpool* pool, size_t order)
{
    struct pmalloc_simple* alloc;
    struct llist_header* bucket;
    struct ppage* good_page;

    assert(order <= NR_PAGE_ORDERS);
    
    alloc = __get_allocator(pool);
    bucket = &alloc->idle_order[order];

    if (!llist_empty(bucket)) 
    {
        (alloc->count[order])--;
        good_page = list_entry(bucket->next, struct ppage, sibs);
        llist_delete(&good_page->sibs);
    }
    else {
        good_page = __looknext(pool, order);
    }

    return good_page;
}


static struct ppage*
__do_alloc(int order, struct pmalloc_pol* pol)
{
    if (pol->should_align)
        return __alloc_napot_type(pol->src_pool, order);
    return __looknext(pol->src_pool, order);
}

void
pmalloc_simple_initpool(struct pmpool* pool)
{
    struct pmalloc_simple* alloc;
    
    alloc = __get_allocator(pool);

    for (int i = 0; i < NR_PAGE_ORDERS; i++) {
        llist_init_head(&alloc->idle_order[i]);
        alloc->count[i] = 0;
    }

    struct ppage* pooled_page = pool->first;
    for (; pooled_page <= pool->last; pooled_page++) {
        pooled_page->pol = __PGPOL_NONE;
    }

    alloc->index = 0;
    
    pool->ops = (struct pmpool_ops) {
        .alloc_page = __do_alloc,
        .free_page = __free_one
    };
}

#endif
