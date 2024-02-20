#ifndef __LUNAIX_PMM_H
#define __LUNAIX_PMM_H
// Physical memory manager

#include <lunaix/boot_generic.h>
#include <lunaix/mm/physical.h>
#include <lunaix/types.h>
#include <stddef.h>
#include <stdint.h>

#define PM_PAGE_SIZE 4096
#define PM_BMP_MAX_SIZE (1024 * 1024)

#define PMM_NORMAL          0
#define PMM_IN_INIT         1
#define PMM_NEED_REINDEX    2

enum {
    POOL_UNIFIED,
    POOL_COUNT
};

typedef unsigned int pp_attr_t;

// Maximum non-huge page order.
#define MAX_PAGE_ORDERS ( LEVEL_SHIFT - 1 )

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

    int state;
    pfn_t size;
    struct ppage* pplist;
    struct llist_header reserved;
};


static inline struct ppage*
ppage(pfn_t pfn) {
    return (struct ppage*)(PPLIST_STARTVM + pfn);
}

static inline struct ppage*
ppage_of(struct pmem_pool* pool, pfn_t pfn) {
    return pool->pool_start + pfn;
}

static inline pfn_t
ppfn(struct ppage* page) {
    return (pfn_t)((ptr_t)page - PPLIST_STARTVM);
}

static inline pfn_t
ppfn_of(struct pmem_pool* pool, struct ppage* page) {
    return (pfn_t)((ptr_t)page - (ptr_t)pool->pool_start);
}

/**
 * @brief 初始化物理内存管理器
 *
 * @param mem_upper_lim 最大可用内存地址
 */
void
pmm_init_begin(struct boot_handoff* bctx);

void
pmm_init_freeze_range(ptr_t start, size_t n);

void
pmm_init_end();

void
pmm_arch_init_begin(struct pmem* memory, struct boot_handoff* bctx);

struct pmem_pool*
pmm_pool_get(int pool_index);

static inline struct pmem_pool*
pmm_pool_lookup(struct ppage* page)
{
    return pmm_pool_get(page->pool);
}

void
pmm_arch_init_pool(struct pmem* memory);

static inline void
take_page(struct ppage* page)
{
    page = leading_page(page);
    assert(page->refs);
    page->refs++;
}

static inline void
return_page(struct ppage* page)
{
    page = leading_page(page);
    assert(page->refs);
    --page->refs ?: pmm_free_one(page, 0);
}

static inline void
change_page_type(struct ppage* page, pp_attr_t type)
{
    page->type = type;
}


// ---- allocator specific ----

void
pmm_allocator_init(struct pmem* memory);

void
pmm_build_free_list(struct pmem_pool* pool);

void
pmm_free_one(struct ppage* page, int type_mask);

struct ppage*
pmm_alloc_napot_type(int pool, size_t order, pp_attr_t attr);

static inline struct ppage*
leading_page(struct ppage* page) {
    return ppage(page);
}

// ---- 


/**
 * @brief 标注物理页为可使用
 *
 * @param ppn page number
 */
void
pmm_reclaim_range(ptr_t ppn, pp_attr_t attr);

/**
 * @brief 标注物理页为已占用
 *
 * @param ppn
 */
void
pmm_acclaim_range(ptr_t ppn, pp_attr_t attr);

/**
 * @brief 标注多个连续的物理页为可用
 *
 * @param start_ppn 起始PPN
 * @param page_count 数量
 */
void
pmm_unreserve_range(ptr_t start_ppn, size_t page_count);

/**
 * @brief 标注多个连续的物理页为已占用
 *
 * @param start_ppn 起始PPN
 * @param page_count 数量
 */
void
pmm_reserve_range(u32_t start_ppn, size_t page_count);

/**
 * @brief 分配一个可用的物理页
 *
 * @return void* 可用的页地址，否则为 NULL
 */
ptr_t
pmm_alloc_page(pp_attr_t attr);

/**
 * @brief 分配一个连续的物理内存区域
 *
 * @param owner
 * @param num_pages 区域大小，单位为页
 * @param attr
 * @return ptr_t
 */
ptr_t
pmm_alloc_cpage(size_t num_pages, pp_attr_t attr);



struct pp_struct*
pmm_query(ptr_t pa);

/**
 * @brief Free a normal physical page
 *
 * @param page 页地址
 * @return 是否成功
 */
static inline int
pmm_free_page(ptr_t page)
{
    return pmm_free_one(page, 0);
}

/**
 * @brief Free physical page regardless of it's attribute
 * 
 * @param page 
 * @return int 
 */
static inline int
pmm_free_any(ptr_t page)
{
    return pmm_free_one(page, -1);
}

int
pmm_ref_page(ptr_t page);

void
pmm_set_attr(ptr_t page, pp_attr_t attr);

#endif /* __LUNAIX_PMM_H */
