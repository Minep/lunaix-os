#include <lunaix/mm/fault.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/region.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/sched.h>
#include <lunaix/signal.h>
#include <lunaix/status.h>
#include <lunaix/syslog.h>
#include <lunaix/trace.h>
#include <lunaix/pcontext.h>

#include <sys/mm/mm_defs.h>

#include <klibc/string.h>

LOG_MODULE("pf")

static void
__gather_memaccess_info(struct fault_context* context)
{
    pte_t* ptep = (pte_t*)context->fault_va;
    ptr_t mnt = ptep_vm_mnt(ptep);
    ptr_t refva;
    
    context->mm = vmspace(__current);

    if (mnt < VMS_MOUNT_1) {
        refva = (ptr_t)ptep;
        goto done;
    }

    context->ptep_fault = true;
    context->remote_fault = (mnt != VMS_SELF);
    
    if (context->remote_fault && context->mm) {
        context->mm = context->mm->guest_mm;
        assert(context->mm);
    }

#if LnT_ENABLED(1)
    ptep = (pte_t*)page_addr(ptep_pfn(ptep));
    mnt  = ptep_vm_mnt(ptep);
    if (mnt < VMS_MOUNT_1) {
        refva = (ptr_t)ptep;
        goto done;
    }
#endif

#if LnT_ENABLED(2)
    ptep = (pte_t*)page_addr(ptep_pfn(ptep));
    mnt  = ptep_vm_mnt(ptep);
    if (mnt < VMS_MOUNT_1) {
        refva = (ptr_t)ptep;
        goto done;
    }
#endif

#if LnT_ENABLED(3)
    ptep = (pte_t*)page_addr(ptep_pfn(ptep));
    mnt  = ptep_vm_mnt(ptep);
    if (mnt < VMS_MOUNT_1) {
        refva = (ptr_t)ptep;
        goto done;
    }
#endif

    ptep = (pte_t*)page_addr(ptep_pfn(ptep));
    mnt  = ptep_vm_mnt(ptep);
    
    assert(mnt < VMS_MOUNT_1);
    refva = (ptr_t)ptep;

done:
    context->fault_refva = refva;
}

static bool
__prepare_fault_context(struct fault_context* fault)
{
    if (!__arch_prepare_fault_context(fault)) {
        return false;
    }

    __gather_memaccess_info(fault);

    pte_t* fault_ptep      = fault->fault_ptep;
    ptr_t  fault_va        = fault->fault_va;
    pte_t  fault_pte       = *fault_ptep;
    bool   kernel_vmfault  = kernel_addr(fault_va);
    bool   kernel_refaddr  = kernel_addr(fault->fault_refva);
    
    // for a ptep fault, the parent page tables should match the actual
    //  accesser permission
    if (kernel_refaddr) {
        ptep_alloc_hierarchy(fault_ptep, fault_va, KERNEL_DATA);
    } else {
        ptep_alloc_hierarchy(fault_ptep, fault_va, USER_DATA);
    }

    fault->fault_pte = fault_pte;
    
    if (fault->ptep_fault && !kernel_refaddr) {
        fault->resolving = pte_setprot(fault_pte, USER_DATA);
    } else {
        fault->resolving = pte_setprot(fault_pte, KERNEL_DATA);
    }

    fault->resolving = pte_mkloaded(fault->resolving);
    fault->kernel_vmfault = kernel_vmfault;
    fault->kernel_access  = kernel_context(fault->ictx);

    return true;
}

static inline void
__flush_staled_tlb(struct fault_context* fault, struct leaflet* leaflet)
{
    tlb_flush_mm_range(fault->mm, fault->fault_va, leaflet_nfold(leaflet));
}

static void
__handle_conflict_pte(struct fault_context* fault) 
{
    pte_t pte;
    struct leaflet *fault_leaflet, *duped_leaflet;

    pte = fault->fault_pte;
    fault_leaflet = pte_leaflet(pte);
    
    if (!pte_allow_user(pte)) {
        return;
    }

    assert(pte_iswprotect(pte));

    if (writable_region(fault->vmr)) {
        // normal page fault, do COW
        duped_leaflet = dup_leaflet(fault_leaflet);

        pte = pte_mkwritable(pte);
        pte = pte_mkuntouch(pte);
        pte = pte_mkclean(pte);

        ptep_map_leaflet(fault->fault_ptep, pte, duped_leaflet);
        __flush_staled_tlb(fault, duped_leaflet);

        leaflet_return(fault_leaflet);

        fault_resolved(fault, NO_PREALLOC);
    }

    return;
}


static void
__handle_anon_region(struct fault_context* fault)
{
    pte_t pte = fault->resolving;
    pte_attr_t prot = region_pteprot(fault->vmr);
    pte = pte_setprot(pte, prot);

    // TODO Potentially we can get different order of leaflet here
    struct leaflet* region_part = alloc_leaflet(0);
    
    ptep_map_leaflet(fault->fault_ptep, pte, region_part);
    __flush_staled_tlb(fault, region_part);

    fault_resolved(fault, NO_PREALLOC);
}


static void
__handle_named_region(struct fault_context* fault)
{
    struct mm_region* vmr = fault->vmr;
    struct v_file* file = vmr->mfile;

    pte_t pte       = fault->resolving;
    ptr_t fault_va  = page_aligned(fault->fault_va);

    u32_t mseg_off  = (fault_va - vmr->start);
    u32_t mfile_off = mseg_off + vmr->foff;

    // TODO Potentially we can get different order of leaflet here
    struct leaflet* region_part = alloc_leaflet(0);

    pte = pte_setprot(pte, region_pteprot(vmr));
    ptep_map_leaflet(fault->fault_ptep, pte, region_part);

    int errno = file->ops->read_page(file->inode, (void*)fault_va, mfile_off);
    if (errno < 0) {
        ERROR("fail to populate page (%d)", errno);

        ptep_unmap_leaflet(fault->fault_ptep, region_part);
        leaflet_return(region_part);

        return;
    }

    __flush_staled_tlb(fault, region_part);

    fault_resolved(fault, NO_PREALLOC);
}

static void
__handle_kernel_page(struct fault_context* fault)
{
    // we must ensure only ptep fault is resolvable
    if (fault->fault_va < VMS_MOUNT_1) {
        return;
    }
    
    struct leaflet* leaflet = fault->prealloc;

    pin_leaflet(leaflet);
    leaflet_wipe(leaflet);
    
    pte_t pte = fault->resolving;
    ptep_map_leaflet(fault->fault_ptep, pte, leaflet);

    tlb_flush_kernel_ranged(fault->fault_va, leaflet_nfold(leaflet));

    fault_resolved(fault, 0);
}


static void
fault_prealloc_page(struct fault_context* fault)
{
    if (!pte_isnull(fault->fault_pte)) {
        return;
    }

    pte_t pte;
    
    struct leaflet* leaflet = alloc_leaflet(0);
    if (!leaflet) {
        return;
    }

    fault->prealloc = leaflet;
}


static void noret
__fail_to_resolve(struct fault_context* fault)
{
    if (fault->prealloc) {
        leaflet_return(fault->prealloc);
    }

    ERROR("(pid: %d) Segmentation fault on %p (%p,e=0x%x)",
          __current->pid,
          fault->fault_va,
          fault->fault_instn,
          fault->fault_data);

    trace_printstack_isr(fault->ictx);

    if (fault->kernel_access) {
        // if a page fault from kernel is not resolvable, then
        //  something must be went south
        FATAL("unresolvable page fault");
        unreachable;
    }

    thread_setsignal(current_thread, _SIGSEGV);

    schedule();
    fail("Unexpected return from segfault");

    unreachable;
}

static bool
__try_resolve_fault(struct fault_context* fault)
{
    pte_t fault_pte = fault->fault_pte;
    if (pte_isguardian(fault_pte)) {
        ERROR("memory region over-running");
        return false;
    }

    if (fault->kernel_vmfault && fault->kernel_access) {
        __handle_kernel_page(fault);
        goto done;
    }

    assert(fault->mm);
    vm_regions_t* vmr = &fault->mm->regions;
    fault->vmr = region_get(vmr, fault->fault_va);

    if (!fault->vmr) {
        return false;
    }

    if (pte_isloaded(fault_pte)) {
        __handle_conflict_pte(fault);
    }
    else if (anon_region(fault->vmr)) {
        __handle_anon_region(fault);
    }
    else if (fault->vmr->mfile) {
        __handle_named_region(fault);
    }
    else {
        // page not present, might be a chance to introduce swap file?
        ERROR("WIP page fault route");
    }
    
done:
    return !!(fault->resolve_type & RESOLVE_OK);
}

void
intr_routine_page_fault(const isr_param* param)
{
    if (param->depth > 10) {
        // Too many nested fault! we must messed up something
        // XXX should we failed silently?
        spin();
    }

    struct fault_context fault = { .ictx = param };

    if (!__prepare_fault_context(&fault)) {
        __fail_to_resolve(&fault);
    }

    fault_prealloc_page(&fault);

    if (!__try_resolve_fault(&fault)) {
        __fail_to_resolve(&fault);
    }

    if ((fault.resolve_type & NO_PREALLOC)) {
        if (fault.prealloc) {
            leaflet_return(fault.prealloc);
        }
    }
}