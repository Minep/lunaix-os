#ifndef __LUNAIX_FAULT_H
#define __LUNAIX_FAULT_H

#include <lunaix/mm/mm.h>
#include <lunaix/pcontext.h>
#include <lunaix/mm/procvm.h>

#define RESOLVE_OK      ( 0b000001 )
#define NO_PREALLOC     ( 0b000010 )

struct fault_context
{
    isr_param* ictx;

    pte_t* fault_ptep;
    pte_t fault_pte;
    ptr_t fault_pa;
    ptr_t fault_va;
    ptr_t fault_refva;
    ptr_t fault_data;
    ptr_t fault_instn;

    pte_t resolving;

    ptr_t prealloc_pa;

    
    bool kernel_vmfault:1;          // faulting address that is kernel
    bool kernel_ref_vmfault:1;      // referenced faulting address is kernel
    bool ptep_fault:1;              // faulting address is a ptep
    bool remote_fault:1;            // referenced faulting address is remote vms
    bool kernel_access:1;           // access cause page fault is from kernel
    struct mm_region* vmr;
    struct proc_mm* mm;

    int resolve_type;
};

bool
fault_populate_core_state(struct fault_context* context);

static inline void
fault_resolved(struct fault_context* fault, pte_t resolved, int flags)
{
    fault->resolving = resolved;
    fault->resolve_type |= (flags | RESOLVE_OK);
}
#endif /* __LUNAIX_FAULT_H */
