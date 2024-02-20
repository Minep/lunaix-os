#ifndef __LUNAIX_PMM_ALLOC_H
#define __LUNAIX_PMM_ALLOC_H

#include <lunaix/mm/pmm.h>

#define RESERVE_MARKER 0xf0f0f0f0

static inline bool
reserved_page(struct ppage* page)
{
    return page->refs == RESERVE_MARKER && page->type == PP_RESERVED;
}

void
pmm_allocator_add_freehole(struct pmem_pool* pool, struct ppage* start, struct ppage* end);

#endif /* __LUNAIX_PMM_ALLOC_H */
