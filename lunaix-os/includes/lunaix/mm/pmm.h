#ifndef __LUNAIX_PMM_H
#define __LUNAIX_PMM_H
// Physical memory manager

#include <lunaix/mm/pagetable.h>
#include <lunaix/mm/pgpol.h>
#include <lunaix/mm/pmalloc.h>
#include <lunaix/mm/physical.h>
#include <lunaix/boot_generic.h>
#include <lunaix/types.h>
#include <lunaix/spike.h>

enum pmpool_type {
    POOL_NORMAL,
    POOL_USER,
    POOL_DMA,
    POOL_COUNT
};

struct pmem
{
    struct pmpool* pool[POOL_COUNT];

    pfn_t list_len;
    struct ppage* pplist;
    struct llist_header reserved;
};

struct pmpool_create_param
{
    struct pmpool* container;
    ptr_t start_addr;
    size_t span;
    enum pmalloc_backend manager;
};

/**
 * @brief 初始化物理内存管理器
 *
 * @param mem_upper_lim 最大可用内存地址
 */
void
pmm_init(struct boot_handoff* bctx);

ptr_t
pmm_arch_init_remap(struct pmem* memory, struct boot_handoff* bctx);

void
pmm_arch_init_pool(struct pmem* memory, struct boot_handoff* bctx);

void
pmm_onhold_range(pfn_t start, size_t npages);

void
pmm_unhold_range(pfn_t start, size_t npages);

bool
pmm_is_reserved_range(pfn_t start, size_t npages);

void
pmm_declare_pool(enum pmpool_type type, struct pmpool_create_param* param);

void
pmm_alias_pool(enum pmpool_type src_pool, enum pmpool_type dest_pool);

void
pmm_explain_policy(struct pmalloc_pol* pol_out, pgpol_t pol);

// ---- allocator specific ----

void
pmm_free_one(struct ppage* page);

static inline struct ppage*
pmm_try_alloc_one(int order, struct pmalloc_pol* policy)
{
    struct pmpool* pool = policy->src_pool;
    return pool->ops.alloc_page(order, policy);
}
#endif /* __LUNAIX_PMM_H */
