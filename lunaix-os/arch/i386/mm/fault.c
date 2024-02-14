#include <lunaix/mm/fault.h>
#include <lunaix/mm/region.h>
#include <lunaix/process.h>
#include <lunaix/pcontext.h>

#include <sys/mm/mm_defs.h>

/**
 * @brief Ensure the page table hierarchy to ptep
 *        is properly allocated, this is to reduce
 *        the number of page faults when getting pte
 * 
 * @param ptep 
 * @param va 
 */
static inline void
fault_ensure_ptep_hierarchy(pte_t* ptep, ptr_t va, bool kernel_faul)
{
    pte_t* _ptep;
    pte_attr_t prot = !kernel_faul ? USER_DATA : KERNEL_DATA;
    
    _ptep = mkl0tep(ptep);
    if (_ptep == ptep) {
        return;
    }

    _ptep = mkl1t(_ptep, va, prot);
    if (_ptep == ptep) {
        return;
    }

    _ptep = mkl2t(_ptep, va, prot);
    if (_ptep == ptep) {
        return;
    }

    _ptep = mkl3t(_ptep, va, prot);
    if (_ptep == ptep) {
        return;
    }

    _ptep = mklft(_ptep, va, prot);
    assert(_ptep == ptep);
}

bool
arch_fault_get_context(struct fault_context* context)
{
    isr_param* ictx = context->ictx;
    ptr_t ptr = cpu_ldeaddr();
    if (!ptr) {
        return false;
    }

    bool kernel_fault   = kernel_addr(ptr);
    pte_t* fault_ptep   = mkptep_va(VMS_SELF, ptr);
    fault_ensure_ptep_hierarchy(fault_ptep, ptr, kernel_fault);

    pte_t fault_pte     = *fault_ptep;

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