#ifndef __LUNAIX_REGION_H
#define __LUNAIX_REGION_H

#include <lunaix/mm/mm.h>

void
region_add(struct mm_region* proc,
           unsigned long start,
           unsigned long end,
           unsigned int attr);

void
region_release_all(struct mm_region* proc);

struct mm_region*
region_get(struct mm_region* proc, unsigned long vaddr);

void
region_copy(struct mm_region* src, struct mm_region* dest);

#endif /* __LUNAIX_REGION_H */
