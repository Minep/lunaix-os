#ifndef __LUNAIX_PMM_H
#define __LUNAIX_PMM_H
// Physical memory manager

#include <lunaix/boot_generic.h>
#include <lunaix/mm/physical.h>
#include <lunaix/types.h>
#include <lunaix/spike.h>

enum {
    POOL_UNIFIED,
    POOL_COUNT
};

typedef unsigned int ppage_type_t;

// Maximum non-huge page order.
#define MAX_PAGE_ORDERS ( LEVEL_SHIFT - 1 )

#define RESERVE_MARKER 0xf0f0f0f0

struct pmem_pool
{
    int type;
    struct ppage* pool_start;
    struct ppage* pool_end;
    
#if defined(CONFIG_PMALLOC_NCONTIG)

    struct llist_header idle_page;
    struct llist_header busy_page;
    
#elif defined(CONFIG_PMALLOC_BUDDY)

    struct llist_header idle_order[MAX_PAGE_ORDERS];
    
#elif defined(CONFIG_PMALLOC_SIMPLE)

    struct llist_header idle_order[MAX_PAGE_ORDERS];
    int count[MAX_PAGE_ORDERS];

#endif
};

struct pmem
{
    struct pmem_pool pool[POOL_COUNT];

    pfn_t list_len;
    struct ppage* pplist;
    struct llist_header reserved;
};

static inline struct ppage*
ppage(pfn_t pfn) 
{
    return (struct ppage*)(PPLIST_STARTVM) + pfn;
}

static inline struct ppage*
leading_page(struct ppage* page) {
    return page - page->companion;
}

static inline struct ppage*
ppage_of(struct pmem_pool* pool, pfn_t pfn) 
{
    return pool->pool_start + pfn;
}

static inline pfn_t
ppfn(struct ppage* page) 
{
    return (pfn_t)((ptr_t)page - PPLIST_STARTVM) / sizeof(struct ppage);
}

static inline pfn_t
ppfn_of(struct pmem_pool* pool, struct ppage* page) 
{
    return (pfn_t)((ptr_t)page - (ptr_t)pool->pool_start) / sizeof(struct ppage);
}

static inline ptr_t
ppage_addr(struct ppage* page) {
    return ppfn(page) * PAGE_SIZE;
}

static inline unsigned int
count_order(size_t page_count) {
    unsigned int po = ILOG2(page_count);
    assert(!(page_count % (1 << po)));
    return po;
}

static inline unsigned int
ppage_order(struct ppage* page) {
    return page->order;
}


static inline bool
reserved_page(struct ppage* page)
{
    return page->refs == RESERVE_MARKER && page->type == PP_RESERVED;
}


/**
 * @brief 初始化物理内存管理器
 *
 * @param mem_upper_lim 最大可用内存地址
 */
void
pmm_init(struct boot_handoff* bctx);

ptr_t
pmm_arch_init_remap(struct pmem* memory, struct boot_handoff* bctx);

struct pmem_pool*
pmm_pool_get(int pool_index);

void
pmm_arch_init_pool(struct pmem* memory);

bool
pmm_onhold_range(pfn_t start, size_t npages);

bool
pmm_unhold_range(pfn_t start, size_t npages);


struct pmem_pool*
pmm_declare_pool(int pool, pfn_t start, pfn_t size);

// ---- allocator specific ----

void
pmm_free_one(struct ppage* page, int type_mask);

struct ppage*
pmm_alloc_napot_type(int pool, size_t order, ppage_type_t type);

// ---- 

static inline struct ppage*
pmm_alloc_normal(size_t order)
{
    return pmm_alloc_napot_type(POOL_UNIFIED, order, 0);
}

static inline struct ppage*
pmm_alloc_locked(size_t order)
{
    return pmm_alloc_napot_type(POOL_UNIFIED, order, PP_FGLOCKED);
}

static inline void
change_page_type(struct ppage* page, ppage_type_t type)
{
    page->type = type;
}

static inline struct pmem_pool*
pmm_pool_lookup(struct ppage* page)
{
    return pmm_pool_get(page->pool);
}


#endif /* __LUNAIX_PMM_H */
