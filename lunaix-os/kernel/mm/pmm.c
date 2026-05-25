#include <lunaix/mm/pmm.h>
#include <lunaix/status.h>
#include <lunaix/mm/pagetable.h>
#include <lunaix/spike.h>
#include <lunaix/owloysius.h>
#include <lunaix/syslog.h>
#include <lunaix/mm/page.h>
#include <lunaix/compiler.h>

LOG_MODULE("pmm")

static struct pmem memory;
export_symbol(debug, pmm, memory);

void
pmm_init(struct boot_handoff* bctx)
{
    ptr_t pplist_pa;

    memset(&memory, 0, sizeof(memory));

    llist_init_head(&memory.reserved);

    pplist_pa = pmm_arch_init_remap(&memory, bctx);
    
    if (!pplist_pa) {
        spin();
    }

    pmm_arch_init_pool(&memory, bctx);

    pfn_t pplist_size = memory.list_len * sizeof(struct ppage);
    pmm_onhold_range(page_index(pplist_pa), count_pages(pplist_size));
}

static inline void  must_inline
__reserve_memory_range(struct ppage* start, struct ppage* end)
{
    while (start <= end) {
        start->pol = __PGPOL_RESERVED; 
        start++;
    }
}

static inline void must_inline
__unreserve_memory_range(struct ppage* start, struct ppage* end)
{
    while (start <= end) {
        start->pol = __PGPOL_NONE;
        start++;
    }
}

static inline void must_inline optimize("-fipa-cp-clone")
__pmm_mark_range(pfn_t start, size_t npages, const bool hold)
{
    struct ppage *_start, *_end;

    if (start >= memory.list_len)
        return;

    _start = ppage(start);
    _end = ppage(start + npages - 1);
    
    if (hold) {
        __reserve_memory_range(_start, _end);
    } else {
        __unreserve_memory_range(_start, _end);
    }
}

void
pmm_onhold_range(pfn_t start, size_t npages)
{
    __pmm_mark_range(start, npages, true);
}

void
pmm_unhold_range(pfn_t start, size_t npages)
{
    __pmm_mark_range(start, npages, false);
}

bool
pmm_is_reserved_range(pfn_t start, size_t npages)
{
    struct ppage *_start, *_end;

    if (start >= memory.list_len)
        return false;

    _start = ppage(start);
    _end = ppage(start + npages - 1);

    while (_start <= _end) {
        if (!reserved_page(_start))
            return false;
        _start++;
    }

    return true;
}

void
pmm_free_one(struct ppage* page)
{
    struct pmpool* pool;
    assert(page->pool < POOL_COUNT);

    page = leading_page(page);

    assert(page->refs);
    assert(!reserved_page(page));

    if (--page->refs) {
        return;
    }

    pool = memory.pool[page->pool];
    pool->ops.free_page(pool, page);
}

void
pmm_declare_pool(enum pmpool_type type, struct pmpool_create_param* param)
{
    struct pmpool* pool;
    pfn_t start;

    assert(memory.pool[type] == NULL);
    pool = param->container;

    start = page_index(param->start_addr);

    pool->first = ppage(start);
    pool->last = ppage(start + param->span);

    pool->type = type;
    
    switch(param->manager) {
#ifdef CONFIG_PMALLOC_METHOD_NCONTIG
        case PMALLOC_NCONTIG:
            pmalloc_ncontig_initpool(pool);
            break;
#endif
#ifdef CONFIG_PMALLOC_METHOD_BUDDY
        case PMALLOC_BUDDY:
            pmalloc_buddy_initpool(pool);
            break;
#endif
#ifdef CONFIG_PMALLOC_METHOD_SIMPLE
        case PMALLOC_SIMPLE:
            pmalloc_simple_initpool(pool);
            break;
#endif
        default:
            fail("unknown pmem manager backend");
    }

    memory.pool[type] = pool;
}

void
pmm_alias_pool(enum pmpool_type src_pool, enum pmpool_type dest_pool)
{
    assert(memory.pool[src_pool] != NULL);
    assert(memory.pool[dest_pool] == NULL);

    memory.pool[dest_pool] = memory.pool[src_pool];
}

void
pmm_explain_policy(struct pmalloc_pol *pol_out, pgpol_t pol)
{
    int kind;

    kind = (pol & PGPOL_KIND_MASK);
    switch (kind)
    {
        case PGPOL_NORMAL:
            pol_out->src_pool = memory.pool[POOL_NORMAL];
            break;
        case PGPOL_NORMAL_USER:
            pol_out->src_pool = memory.pool[POOL_USER];
            break;
        case PGPOL_PCACHE:
        case PGPOL_DMA:
            // TODO [2026-DMA] survey on DMA.
            pol_out->src_pool = memory.pool[POOL_NORMAL];
            break;

        default:
            pol_out->src_pool = NULL;
            return;
    }

    pol_out->panic_on_fail = !!(pol & PGPOL_NOFAIL);
    pol_out->should_zero = !!(pol & PGPOL_ZERO);
    pol_out->should_align = !(pol & PGPOL_NNAPOT);

    if ((pol & PGPOL_NORETRY)) {
        pol_out->max_attempt = 0;
    } else {
        pol_out->max_attempt = 3;
    }
    
    pol_out->policy = pol;
}

static void
pmm_log_summary()
{
    pfn_t len;
    struct pmpool* _pool;

    INFO("init: nr_pages=%ld, gran=0x%lx", memory.list_len, PAGE_SIZE);

    for (int i = 0; i < POOL_COUNT; i++)
    {
        _pool = memory.pool[i];
        len   = ppfn(_pool->last) - ppfn(_pool->first) + 1;
        
        INFO("pool #%d (%d), 0x%lx..0x%lx (span: %ld pages)", 
                i , _pool->type, 
                ppfn(_pool->first) * PAGE_SIZE, ppfn(_pool->last) * PAGE_SIZE, len);
    }
}
owloysius_fetch_init(pmm_log_summary, on_sysconf);
