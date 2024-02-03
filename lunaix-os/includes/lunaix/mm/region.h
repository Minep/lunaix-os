#ifndef __LUNAIX_REGION_H
#define __LUNAIX_REGION_H

#include <lunaix/mm/mm.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/procvm.h>

static inline int
stack_region(struct mm_region* region) {
    return region->attr & REGION_TYPE_STACK;
}

static inline int
same_region(struct mm_region* a, struct mm_region* b) {
    return a->start == b->start \
            && a->end == b->end \
            && a->attr == b->attr;
}

static inline bool
region_contains(struct mm_region* mm, ptr_t va) {
    return mm->start <= va && va < mm->end;
}

static inline size_t
region_size(struct mm_region* mm) {
    return mm->end - mm->start;
}


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

static u32_t
region_ptattr(struct mm_region* vmr)
{
    u32_t vmr_attr = vmr->attr;
    u32_t ptattr = PG_PRESENT | PG_ALLOW_USER;

    if ((vmr_attr & PROT_WRITE)) {
        ptattr |= PG_WRITE;
    }

    return ptattr & 0xfff;
}

#endif /* __LUNAIX_REGION_H */
