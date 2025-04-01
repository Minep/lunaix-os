#include <lunaix/status.h>
#include <lunaix/mm/pagetable.h>
#include <lunaix/spike.h>
#include <lunaix/owloysius.h>
#include <lunaix/syslog.h>

#include "pmm_internal.h"

LOG_MODULE("pmm")

static inline bool
__check_typemask(struct ppage* page, ppage_type_t typemask)
{
    return !page->type || (page->type & typemask);
}

static struct pmem memory;
export_symbol(debug, pmm, memory);

void
pmm_init(struct boot_handoff* bctx)
{
    ptr_t pplist_pa;

    llist_init_head(&memory.reserved);

    pplist_pa = pmm_arch_init_remap(&memory, bctx);
    
    if (!pplist_pa) {
        spin();
    }

    pmm_arch_init_pool(&memory);

    pmm_allocator_init(&memory);

    for (size_t i = 0; i < POOL_COUNT; i++)
    {
        pmm_allocator_init_pool(&memory.pool[i]);
    }

    pfn_t pplist_size = memory.list_len * sizeof(struct ppage);
    pmm_onhold_range(pfn(pplist_pa), leaf_count(pplist_size));
}

static inline bool must_inline optimize("-fipa-cp-clone")
__pmm_mark_range(pfn_t start, size_t npages, const bool hold)
{
    if (start >= memory.list_len) {
        return true;
    }

    struct ppage *_start, *_end, 
                 *_mark_start, *_mark_end;

    _start = ppage(start);
    _end = ppage(start + npages - 1);
    
    struct pmem_pool* pool;
    for (int i = 0; npages && i < POOL_COUNT; i++) {
        pool = &memory.pool[i];

        _mark_start = MAX(pool->pool_start, _start);
        _mark_end   = MIN(pool->pool_end, _end);
        if (pool->pool_end < _mark_start || _mark_end < pool->pool_start) {
            continue;
        }

        bool _r;
        if (hold) {
            _r = pmm_allocator_trymark_onhold(pool, _mark_start, _mark_end);
        } else {
            _r = pmm_allocator_trymark_unhold(pool, _mark_start, _mark_end);
        }

        if (_r)
        {
            npages -= (ppfn(_mark_end) - ppfn(_mark_start)) + 1;
        }
    }

    return !npages;
}

bool
pmm_onhold_range(pfn_t start, size_t npages)
{
    return __pmm_mark_range(start, npages, true);
}

bool
pmm_unhold_range(pfn_t start, size_t npages)
{
    return __pmm_mark_range(start, npages, false);
}

struct pmem_pool*
pmm_pool_get(int pool_index)
{
    assert(pool_index < POOL_COUNT);

    return &memory.pool[pool_index];
}

struct pmem_pool*
pmm_declare_pool(int pool, pfn_t start, pfn_t size)
{
    struct pmem_pool* _pool = &memory.pool[pool];

    _pool->type = POOL_UNIFIED;
    _pool->pool_end = ppage(start + size - 1);
    _pool->pool_start = ppage(start);

    return _pool;
}

static void
pmm_log_summary()
{
    pfn_t len;
    struct pmem_pool* _pool;

    INFO("init: nr_pages=%ld, gran=0x%lx", memory.list_len, 1 << PAGE_SHIFT);

    for (int i = 0; i < POOL_COUNT; i++)
    {
        _pool = &memory.pool[i];
        len   = ppfn(_pool->pool_end) - ppfn(_pool->pool_start) + 1;
        
        INFO("pool #%d (%d), %ld-%ld(0x%lx)", 
                i , _pool->type, 
                ppfn(_pool->pool_start), ppfn(_pool->pool_end), len);
    }
}
owloysius_fetch_init(pmm_log_summary, on_sysconf);