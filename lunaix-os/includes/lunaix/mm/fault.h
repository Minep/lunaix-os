#ifndef __LUNAIX_FAULT_H
#define __LUNAIX_FAULT_H

#include "lunaix/mm/vastm.h"
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
        ptr_t fault_va;
        ptr_t fault_data;
        ptr_t fault_instn;
    };                      // arch-dependent fault state

    pte_t fault_pte;        // the fault-causing pte
    
    struct {
        pte_t attr;        // the pte that will resolve the fault
        struct leaflet* leaflet;
        int result;
    } resolved;

    struct leaflet* prealloc;      // preallocated physical page in-case of empty fault-pte
    
    struct 
    {
        bool kernel_vmfault:1;  // faulting address that is kernel
        bool kernel_access:1;   // kernel mem access causing the fault
    };

    ptroot_t fault_vms;
    struct proc_mm* mm;     // process memory space associated with fault, might be remote
    struct mm_region* vmr;

};

static inline void
fault_resolved(struct fault_context* fault, pte_t attr, struct leaflet* resolved)
{
    fault->resolved.attr = attr;
    fault->resolved.leaflet = resolved;

    fault->resolved.result |= RESOLVE_OK;
    if (resolved != fault->prealloc)
        fault->resolved.result |= NO_PREALLOC;
}

bool
handle_page_fault(struct fault_context* fault);

void noret
fault_resolving_failed(struct fault_context* fault);

#endif /* __LUNAIX_FAULT_H */
