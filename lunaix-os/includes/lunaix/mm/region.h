#ifndef __LUNAIX_REGION_H
#define __LUNAIX_REGION_H

#include <lunaix/mm/procvm.h>

struct mm_region*
region_create(ptr_t start, ptr_t end, u32_t attr);

struct mm_region*
region_create_range(ptr_t start, size_t length, u32_t attr);

void
region_add(vm_regions_t* lead, struct mm_region* vmregion);

void
region_release(struct mm_region* region);

void
region_release_all(vm_regions_t* lead);

struct mm_region*
region_get(vm_regions_t* lead, unsigned long vaddr);

void
region_copy_mm(struct proc_mm* src, struct proc_mm* dest);

struct mm_region*
region_dup(struct mm_region* origin);

#endif /* __LUNAIX_REGION_H */
