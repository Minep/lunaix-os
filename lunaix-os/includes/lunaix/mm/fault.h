#ifndef __LUNAIX_FAULT_H
#define __LUNAIX_FAULT_H

#include <lunaix/mm/mm.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/procvm.h>
#include <lunaix/hart_state.h>

#define RESOLVE_OK      ( 0b000001 )
#define NO_PREALLOC     ( 0b000010 )

struct fault_context
{
    struct hart_state* hstate;

    struct 
    {
        pte_t* fault_ptep;
        ptr_t fault_va;
        ptr_t fault_data;
        ptr_t fault_instn;
    };                      // arch-dependent fault state

    pte_t fault_pte;        // the fault-causing pte
    ptr_t fault_refva;      // referneced va, for ptep fault, equals to fault_va otherwise
    pte_t resolving;        // the pte that will resolve the fault

    struct leaflet* prealloc;      // preallocated physical page in-case of empty fault-pte
    
    struct 
    {
        bool kernel_vmfault:1;  // faulting address that is kernel
        bool ptep_fault:1;      // faulting address is a ptep
        bool remote_fault:1;    // referenced faulting address is remote vms
        bool kernel_access:1;   // kernel mem access causing the fault
    };

    struct proc_mm* mm;     // process memory space associated with fault, might be remote
    struct mm_region* vmr;

    int resolve_type;
};

bool
__arch_prepare_fault_context(struct fault_context* context);

static inline void
fault_resolved(struct fault_context* fault, int flags)
{
    fault->resolve_type |= (flags | RESOLVE_OK);
}
#endif /* __LUNAIX_FAULT_H */
