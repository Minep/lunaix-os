#include <lunaix/mm/fault.h>
#include <lunaix/mm/region.h>
#include <lunaix/process.h>
#include <lunaix/pcontext.h>

#include <sys/mm/mm_defs.h>

bool
arch_fault_get_context(struct fault_context* context)
{
    isr_param* ictx = context->ictx;
    ptr_t ptr = cpu_ldeaddr();
    if (!ptr) {
        return false;
    }

    pte_t* fault_ptep   = mkptep_va(VMS_SELF, ptr);
    pte_t fault_pte     = *fault_ptep;
    bool kernel_fault   = kernel_addr(ptr);

    context->fault_ptep   = fault_ptep;
    context->fault_pte    = fault_pte;
    context->fault_pa     = pte_paddr(fault_pte);
    context->fault_va     = ptr;
    context->kernel_fault = kernel_fault;
    context->fault_data   = ictx->execp->err_code;
    context->fault_instn  = ictx->execp->eip;

    context->resolving    = fault_pte;

    if (!__current) {
        return true;
    }

    vm_regions_t* vmr = vmregions(__current);
    context->vmr = region_get(vmr, ptr);

    if (!context->vmr) {
        return kernel_fault;
    }

    return true;
}