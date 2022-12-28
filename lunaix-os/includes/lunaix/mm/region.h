#ifndef __LUNAIX_REGION_H
#define __LUNAIX_REGION_H

#include <lunaix/mm/mm.h>

typedef struct llist_header vm_regions_t;

struct mm_region*
region_create(ptr_t start, ptr_t end, u32_t attr);

void
region_add(vm_regions_t* lead, struct mm_region* vmregion);

void
region_release_all(vm_regions_t* lead);

struct mm_region*
region_get(vm_regions_t* lead, unsigned long vaddr);

void
region_copy(vm_regions_t* src, vm_regions_t* dest);

#endif /* __LUNAIX_REGION_H */
