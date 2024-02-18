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

static ptr_t
__unpack_fault_address(struct fault_context* context, ptr_t fault_ptr)
{
    pte_t* ptep = (pte_t*)fault_ptr;
    ptr_t mnt = ptep_vm_mnt(ptep);
    
    if (__current)
        context->mm = vmspace(__current);

    if (mnt < VMS_MOUNT_1) 
        return fault_ptr;

    context->ptep_fault = true;
    context->remote_fault = (mnt != VMS_SELF);
    
    if (context->remote_fault && context->mm) {
        assert(context->mm->guest_mm);
        context->mm = context->mm->guest_mm;
    }

#if LnT_ENABLED(1)
    ptep = (pte_t*)page_addr(ptep_pfn(ptep));
    mnt  = ptep_vm_mnt(ptep);
    if (mnt < VMS_MOUNT_1) 
        return (ptr_t)ptep;
#endif

#if LnT_ENABLED(2)
    ptep = (pte_t*)page_addr(ptep_pfn(ptep));
    mnt  = ptep_vm_mnt(ptep);
    if (mnt < VMS_MOUNT_1) 
        return (ptr_t)ptep;
#endif

#if LnT_ENABLED(3)
    ptep = (pte_t*)page_addr(ptep_pfn(ptep));
    mnt  = ptep_vm_mnt(ptep);
    if (mnt < VMS_MOUNT_1) 
        return (ptr_t)ptep;
#endif

    ptep = (pte_t*)page_addr(ptep_pfn(ptep));
    mnt  = ptep_vm_mnt(ptep);
    
    assert(mnt < VMS_MOUNT_1);
    return (ptr_t)ptep;
}

// TODO refactor this, not everything down there are arch-dependent!
bool
fault_populate_core_state(struct fault_context* context)
{
    isr_param* ictx = context->ictx;
    ptr_t ptr = cpu_ldeaddr();
    if (!ptr) {
        return false;
    }

    ptr_t refva = __unpack_fault_address(context, ptr);

    bool kernel_vmfault   = kernel_addr(ptr);
    bool kernel_refaddr   = kernel_addr(refva);
    pte_t* fault_ptep     = mkptep_va(VMS_SELF, ptr);
    fault_ensure_ptep_hierarchy(fault_ptep, ptr, kernel_refaddr);

    pte_t fault_pte       = *fault_ptep;

    context->fault_ptep   = fault_ptep;
    context->fault_pte    = fault_pte;
    context->fault_pa     = pte_paddr(fault_pte);
    context->fault_va     = ptr;
    context->fault_refva  = refva;
    context->fault_data   = ictx->execp->err_code;
    context->fault_instn  = ictx->execp->eip;
    
    if (context->ptep_fault && !kernel_refaddr) {
        // for a ptep fault, the resolved pte should match the encoded address
        context->resolving = pte_setprot(fault_pte, USER_DATA);
    } else {
        context->resolving = pte_setprot(fault_pte, KERNEL_DATA);
    }

    context->kernel_vmfault = kernel_vmfault;
    context->kernel_access  = kernel_context(ictx);

    if (kernel_refaddr) {
        return true;
    }

    assert(context->mm);
    vm_regions_t* vmr = &context->mm->regions;
    context->vmr = region_get(vmr, ptr);

    if (!context->vmr) {
        return kernel_vmfault;
    }

    return true;
}