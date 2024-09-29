#ifndef __LUNAIX_ARCH_GENERIC_INIT_PAGETABLE_H
#define __LUNAIX_ARCH_GENERIC_INIT_PAGETABLE_H

#include <lunaix/types.h>
#include <lunaix/mm/pagetable.h>

struct pt_alloc
{
    ptr_t base;
    int index;
    int total;
};

struct ptw_state
{
    struct pt_alloc* alloc;
    pte_t* root;
    pte_t* lntp;
    int tab_index;
    int level;
};

/**
 * Allocate a page table from the page-table pool
 */
ptr_t 
kpt_alloc_table(struct pt_alloc* alloc);

/**
 * Set contiguous number of ptes starting from `addr`
 * Using flattened apporach (i.e., unfold recursive)
 */
unsigned int 
kpt_set_ptes_flatten(struct ptw_state* state, ptr_t addr, 
                pte_t pte, unsigned long lsize, unsigned int nr);

/**
 * Remap the kernel to high-memory
 */
void 
kpt_migrate_highmem(struct ptw_state* state);

static inline void must_inline
init_pt_alloc(struct pt_alloc* alloc, ptr_t pool, unsigned int size)
{
    *alloc = (struct pt_alloc) {
        .base = pool,
        .index = 0,
        .total = size / PAGE_SIZE
    };
}

static inline void must_inline
init_ptw_state(struct ptw_state* state, struct pt_alloc* alloc, ptr_t root)
{
    *state = (struct ptw_state) {
        .alloc = alloc,
        .root  = (pte_t*) root,
        .level = 0
    };
}

/**
 * set_ptes that is ensured to success
 */
static inline void must_inline
kpt_set_ptes(struct ptw_state* state, ptr_t addr, 
                pte_t pte, unsigned long lsize, unsigned int nr)
{
    if (kpt_set_ptes_flatten(state, addr, pte, lsize, nr) != nr) {
        spin();
    }
}

/**
 *  prepare a editable page table covering va range [addr, addr + lsize)
 */
static inline void must_inline
kpt_mktable_at(struct ptw_state* state, ptr_t addr, unsigned long lsize)
{
    kpt_set_ptes(state, addr, null_pte, lsize >> _PAGE_BASE_SHIFT, 1);
}

#endif /* __LUNAIX_ARCH_GENERIC_INIT_PAGETABLE_H */
