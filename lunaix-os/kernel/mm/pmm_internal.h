#ifndef __LUNAIX_PMM_ALLOC_H
#define __LUNAIX_PMM_ALLOC_H

#include <lunaix/mm/pmm.h>

static inline void
set_reserved(struct ppage* page)
{
    page->refs = RESERVE_MARKER;
    page->type = PP_RESERVED;
    page->order = 0;
}

void
pmm_allocator_init(struct pmem* memory);

void
pmm_allocator_init_pool(struct pmem_pool* pool);

void
pmm_allocator_add_freehole(struct pmem_pool* pool, struct ppage* start, struct ppage* end);

bool
pmm_allocator_trymark_onhold(struct pmem_pool* pool, struct ppage* start, struct ppage* end);

bool
pmm_allocator_trymark_unhold(struct pmem_pool* pool, struct ppage* start, struct ppage* end);

#endif /* __LUNAIX_PMM_ALLOC_H */
