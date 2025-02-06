#ifndef __LUNAIX_REGION_H
#define __LUNAIX_REGION_H

#include <lunaix/mm/mm.h>
#include <lunaix/mm/procvm.h>

#define prev_region(vmr) list_prev(vmr, struct mm_region, head)
#define next_region(vmr) list_next(vmr, struct mm_region, head)
#define get_region(vmr_el) list_entry(vmr_el, struct mm_region, head)

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

static inline bool
anon_region(struct mm_region* mm) {
    return (mm->attr & REGION_ANON);
}

static inline bool
writable_region(struct mm_region* mm) {
    return !!(mm->attr & (REGION_RSHARED | REGION_WRITE));
}

static inline bool
readable_region(struct mm_region* mm) {
    return !!(mm->attr & (REGION_RSHARED | REGION_READ));
}

static inline bool
executable_region(struct mm_region* mm) {
    return !!(mm->attr & REGION_EXEC);
}

static inline bool
shared_writable_region(struct mm_region* mm) {
    return !!(mm->attr & REGION_WSHARED);
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

static inline pte_t
region_tweakpte(struct mm_region* vmr, pte_t pte)
{
    return translate_vmr_prot(vmr->attr, pte);
}

#endif /* __LUNAIX_REGION_H */
