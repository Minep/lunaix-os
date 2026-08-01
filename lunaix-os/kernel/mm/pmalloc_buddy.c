#include <lunaix/compiler.h>
#include <lunaix/status.h>
#include <lunaix/syslog.h>
#include <lunaix/types.h>
#include <lunaix/spike.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/pgpol.h>
#include <lunaix/mm/physical.h>
#include <lunaix/mm/pmalloc.h>

#define MAX_BUDDY_ORDER         10

LOG_MODULE("buddy")

struct buddy_stats {
    unsigned int total;
    unsigned int used;
};

struct buddy_slot {
    struct llist_header cache;
    struct buddy_stats stats;
};

struct pmalloc_buddy {
    struct pmpool* pool;
    struct buddy_slot slots[MAX_BUDDY_ORDER + 1];
};


static inline struct pmalloc_buddy*
__get_allocator(struct pmpool* pool) {
    return (struct pmalloc_buddy*)pool->alloc_private;
}

static inline int
__alignment_to_order(unsigned long pfn) {
    int order;

    order = ctzl(pfn);
    return order > MAX_BUDDY_ORDER ? MAX_BUDDY_ORDER : order;
}

static inline void
__ensure_alignment(struct ppage* page, int order) {
    assert_msg(!(ppfn(page) & ~(-1UL << order)), 
                "alignment check failed");
}

static inline void
__return_buddy_to_slot(struct buddy_slot* slot, struct ppage* buddy)
{
    llist_append(&slot->cache, &buddy->sibs);
    slot->stats.used--;
}

static inline struct ppage*
__grab_buddy_from_slot(struct buddy_slot* slot)
{
    struct ppage* page;

    page = list_entry(slot->cache.next, struct ppage, sibs);

    llist_delete(&page->sibs);
    slot->stats.used++;

    assert(page->refs == 0);
    return page;
}

static inline void
__paint_page_to_buddy(struct ppage* start, int order)
{
    struct ppage* pg;

    __ensure_alignment(start, order);
    
    pg = start;
    for (int i = 0; i < (1 << order); i++, pg++) {
        pg->pol = __PGPOL_NONE;
        pg->order = order;
    }

    llist_init_head(&start->sibs);
}

static inline void
__init_alloc(struct pmalloc_buddy* alloc)
{
    struct buddy_slot* slot;

    for (int i = 0; i <= MAX_BUDDY_ORDER; i++) {
        slot = &alloc->slots[i];

        slot->stats = (struct buddy_stats){ };
        llist_init_head(&slot->cache);
    }
}

static inline void
__detach_buddy(struct pmalloc_buddy* alloc, struct ppage* page)
{
    // check whether already an orphaned page
    if (llist_empty(&page->sibs))
        return;

    llist_delete(&page->sibs);
    alloc->slots[page->order].stats.total--;
}

static inline void
__attach_buddy(struct pmalloc_buddy* alloc, struct ppage* page)
{
    struct buddy_slot* slot;

    slot = &alloc->slots[page->order];

    llist_append(&slot->cache, &page->sibs);
    slot->stats.total++;
}

static bool
__split_page(struct ppage* pg_in, struct ppage** left, struct ppage** right)
{
    int new_order;
    struct ppage *_l, *_r;

    if (pg_in->order == 0)
        return false;

    new_order = pg_in->order - 1;

    _l = pg_in;
    __paint_page_to_buddy(_l, new_order);

    _r = &pg_in[1 << new_order];
    __paint_page_to_buddy(_r, new_order);

    *left = _l;
    *right = _r;

    return true;
}


static struct ppage*
__split_towards(struct pmalloc_buddy* alloc, 
                struct ppage* page, int desired_order)
{
    struct ppage *left, *right;

    if (page->order == desired_order) {
        __detach_buddy(alloc, page);
        return page;
    }

    left = page;
    while (left->order > desired_order) {
        __detach_buddy(alloc, left);

        if (!__split_page(left, &left, &right))
            return NULL;

        __attach_buddy(alloc, right);
    }

    return left;
}

static inline struct ppage* 
__alloc_page_slow_path(struct pmalloc_buddy* alloc, int order)
{
    int search_order;
    struct buddy_slot* slot;
    struct ppage *fresh, *page;

    search_order = order + 1;

    while (search_order <= MAX_BUDDY_ORDER) {
        slot = &alloc->slots[search_order];

        if (llist_empty(&slot->cache)) {
            search_order++;
            continue;
        }
        
        page = list_entry(slot->cache.next, struct ppage, sibs);
        fresh = __split_towards(alloc, page, order);

        assert(fresh);
        return fresh;
    }

    return NULL;
}

static inline struct ppage*
__alloc_page_fast_path(struct pmalloc_buddy* alloc, int order)
{
    struct buddy_slot* slot;

    slot = &alloc->slots[order];
    
    if (llist_empty(&slot->cache))
        return NULL;

    return __grab_buddy_from_slot(slot);
}

static inline bool
__merge_page(struct pmalloc_buddy* alloc, struct ppage* page)
{
    struct ppage *buddy, *lead;
    int new_order;
    unsigned long pfn;

    if (page->order == MAX_BUDDY_ORDER)
        return false;
    
    new_order = page->order + 1;
    pfn = ppfn(page);

    lead = page;

    if (!(pfn & ~(-1UL << new_order)))
        buddy = &page[1 << page->order];
    else {
        buddy = ppage(pfn & (-1UL << new_order));
        lead = buddy;
    }

    if (ppfn(buddy) > ppfn(alloc->pool->last))
        return false;
    
    if (ppfn(buddy) < ppfn(alloc->pool->first))
        return false;

    if (buddy->pol != __PGPOL_NONE)
        return false;

    __detach_buddy(alloc, page);
    __detach_buddy(alloc, buddy);

    __paint_page_to_buddy(lead, new_order);
    __attach_buddy(alloc, lead);

    return true;
}

static inline void
__free_page(struct pmalloc_buddy* alloc, struct ppage* page)
{
    struct buddy_slot* slot;
    struct ppage* pos;
    int order;

    order = page->order;
    assert(0 <= order && order <= MAX_BUDDY_ORDER);

    page = leading_page(page);

    if (__merge_page(alloc, page))
        return;

    // In case its buddy is not free or not applicable, 
    // then return to its cache.
    //
    slot = &alloc->slots[page->order];

    foreach_page(page, pos) {
        pos->pol = __PGPOL_NONE;
    }

    __return_buddy_to_slot(slot, page);
}

#define __divide_buddy_pages(page, order, start, end, action)     \
    do {                                                                \
        struct ppage* page;                                             \
        unsigned long __pfn, __end_pfn;                                 \
        int order;                                                      \
                                                                        \
        __pfn = (start);                                                \
        __end_pfn = (end);                                              \
                                                                        \
        while (__pfn < __end_pfn) {                                     \
            page = ppage(__pfn);                                        \
            order = __alignment_to_order(__pfn);                        \
                                                                        \
            while (order >= 0 && __pfn + (1 << order) > __end_pfn) {    \
                order--;                                                \
            }                                                           \
                                                                        \
            assert(order >= 0);                                         \
                                                                        \
            action;                                                     \
                                                                        \
            __pfn += 1 << order;                                        \
        }                                                               \
    } while (0)

static void
__compute_free_buddy_pages(struct pmalloc_buddy* alloc, 
                      unsigned long start_pfn, unsigned long end_pfn)
{
    __divide_buddy_pages(page, order, start_pfn, end_pfn, {
        __paint_page_to_buddy(page, order);
        __attach_buddy(alloc, page);
    });
}

static void
__compute_reserved_buddy_pages(struct pmalloc_buddy* alloc, 
                      unsigned long start_pfn, unsigned long end_pfn)
{
    struct ppage* pg;
    __divide_buddy_pages(page, order, start_pfn, end_pfn, {
        for (int i = 0; i < (1 << order); i++) {
            pg = &page[i];
            pg->pol = __PGPOL_RESERVED;
            pg->order = order;
        }
    });
}

static int
__reserve_range(struct pmalloc_buddy* alloc,
                unsigned long start_pfn, unsigned long end_pfn)
{
    struct ppage *head, *tail;
    struct ppage* pg;
    unsigned long tail_end;

    head = ppage(start_pfn & (-1UL << ppage(start_pfn)->order));
    tail = ppage(end_pfn & (-1UL << ppage(end_pfn)->order));
    tail_end = ppfn(tail) + (1UL << tail->order);

    // step 1. detach all affected buddy pages
    //
    pg = head;
    while (ppfn(pg) <= ppfn(tail)) 
    {
        if (unlikely(pg->pol != __PGPOL_NONE))
            return EBUSY;

        __detach_buddy(alloc, pg);
        pg = &pg[1 << pg->order];
    }

    // step 2. resplit the inner paddings
    //
    __compute_free_buddy_pages(alloc, ppfn(head), start_pfn);
    __compute_free_buddy_pages(alloc, end_pfn, tail_end);
    
    // step 3. convert the range into buddy pages.
    //
    __compute_reserved_buddy_pages(alloc, start_pfn, end_pfn);

    return 0;
}

static int
__unreserve_range(struct pmalloc_buddy* alloc,
                unsigned long start_pfn, unsigned long end_pfn)
{
    struct ppage *head, *tail;
    struct ppage* pg;
    int order;

    head = ppage(start_pfn & (-1UL << ppage(start_pfn)->order));
    tail = ppage(end_pfn & (-1UL << ppage(end_pfn)->order));

    pg = head;
    while (ppfn(pg) <= ppfn(tail)) 
    {
        order = pg->order;
        if (unlikely(!reserved_page(pg)))
            return EINVAL;

        pg = &pg[1 << order];
    }

    // unreserve is the inversion of the reserving steps.

    // in case we are doing partial unreserving
    //
    __compute_reserved_buddy_pages(alloc, ppfn(head), start_pfn);
    __compute_reserved_buddy_pages(alloc, 
            end_pfn, ppfn(tail) + (1UL << tail->order));
    
    // mark the unreserved to be free buddy page and joined the cache.
    // NB. we defered the merge opportunity to __free_page()
    //
    __compute_free_buddy_pages(alloc, start_pfn, end_pfn);

    return 0;
}

static int
__do_reserve(struct pmpool* pool, unsigned long start, unsigned long span)
{
    struct pmalloc_buddy* alloc;

    alloc = __get_allocator(pool);
    return __reserve_range(alloc, start, start + span);
}

static int
__do_unreserve(struct pmpool* pool, unsigned long start, unsigned long span)
{
    struct pmalloc_buddy* alloc;

    alloc = __get_allocator(pool);
    return __unreserve_range(alloc, start, start + span);
}

static struct ppage*
__do_alloc(int order, struct pmalloc_pol* pol)
{
    struct pmpool* pool;
    struct pmalloc_buddy* alloc;
    struct buddy_slot* slot;
    struct ppage* result;

    pool = pol->src_pool;
    alloc = __get_allocator(pool);

    assert_msg(0 <= order && order <= MAX_BUDDY_ORDER, 
                "invalid page order.");
    
    result = __alloc_page_fast_path(alloc, order);
    if (result)
        return result;

    result = __alloc_page_slow_path(alloc, order);
    if (!result)
        return NULL;

    // for book-keeping
    // fast path is already done the book-keeping for us.
    //
    slot = &alloc->slots[order];
    slot->stats.total++;
    slot->stats.used++;
    
    return result;
}

static void
__free_one(struct pmpool* pool, struct ppage* page) 
{
    struct pmalloc_buddy* alloc;

    alloc = __get_allocator(pool);
    __free_page(alloc, page);
}

static void
__log_boottime_summary(struct pmpool* pool)
{
    struct buddy_slot* slot;
    struct pmalloc_buddy* alloc;

    alloc = __get_allocator(pool);

    for (int i = 0; i <= MAX_BUDDY_ORDER; i++)
    {
        slot = &alloc->slots[i];
        INFO("(pool 0x%x) ORDER %d .... %d", 
                pool->id, i, slot->stats.total);
    }
}

static inline void
__initialise_buddy(struct ppage* start, int order, int pool_id)
{
    for (int i = 0; i < (1 << order); i++) {
        start[i] = (struct ppage) {
            .pol = __PGPOL_NONE,
            .order = order
        };
    }
}

void 
pmalloc_buddy_initpool(struct pmpool* pool)
{
    struct pmalloc_buddy* alloc;
    struct buddy_slot* slot;

    unsigned long pfn, max_pfn;

    alloc = __get_allocator(pool);
    pfn = ppfn(pool->first);
    max_pfn = ppfn(pool->last);

    __init_alloc(alloc);
    alloc->pool = pool;

    __divide_buddy_pages(page, order, pfn, max_pfn + 1, {
        __initialise_buddy(page, order, pool->id);
        __attach_buddy(alloc, page);
    });
    
    pool->ops = (struct pmpool_ops) {
        .alloc_page = __do_alloc,
        .free_page = __free_one,
        .log_boottime_summary = __log_boottime_summary,
        .reserve_range = __do_reserve,
        .unreserve_range = __do_unreserve
    };
}
