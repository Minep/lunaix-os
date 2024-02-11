#ifndef __LUNAIX_FAULT_H
#define __LUNAIX_FAULT_H

#include <lunaix/mm/mm.h>
#include <lunaix/pcontext.h>

#define RESOLVE_OK      ( 0b000001 )
#define NO_PREALLOC     ( 0b000010 )

struct fault_context
{
    isr_param* ictx;

    pte_t* fault_ptep;
    pte_t fault_pte;
    ptr_t fault_pa;
    ptr_t fault_va;
    ptr_t fault_data;
    ptr_t fault_instn;

    pte_t resolving;

    ptr_t prealloc_pa;

    bool kernel_fault;
    struct mm_region* vmr;

    int resolve_type;
};

bool
arch_fault_get_context(struct fault_context* context);

static inline void
fault_resolved(struct fault_context* fault, pte_t resolved, int flags)
{
    fault->resolving = resolved;
    fault->resolve_type |= (flags | RESOLVE_OK);
}
#endif /* __LUNAIX_FAULT_H */
