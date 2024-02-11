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
fault_handle_conflict_pte(struct fault_context* fault) 
{
    pte_t pte = fault->fault_pte;
    ptr_t fault_pa  = fault->fault_pa;
    if (!pte_allow_user(pte)) {
        return;
    }

    assert(pte_iswprotect(pte));

    if (writable_region(fault->vmr)) {
        // normal page fault, do COW
        // TODO makes `vmm_dup_page` arch-independent
        ptr_t pa = (ptr_t)vmm_dup_page(fault_pa);

        pmm_free_page(fault_pa);
        pte_t new_pte = pte_setpaddr(pte, pa);
        new_pte = pte_mkwritable(new_pte);

        set_pte(fault->fault_ptep, pte);
        fault_resolved(fault, new_pte, NO_PREALLOC);
    }

    return;
}


static void
fault_handle_anon_region(struct fault_context* fault)
{
    pte_t pte = fault->resolving;
    pte_attr_t prot = region_pteprot(fault->vmr);
    pte = pte_setprot(pte, prot);

    set_pte(fault->fault_ptep, pte);
    fault_resolved(fault, pte, 0);
}


static void
fault_handle_named_region(struct fault_context* fault)
{
    struct mm_region* vmr = fault->vmr;
    struct v_file* file = vmr->mfile;

    pte_t pte       = fault->resolving;
    ptr_t fault_va  = page_addr(fault->fault_va);

    u32_t mseg_off  = (fault_va - vmr->start);
    u32_t mfile_off = mseg_off + vmr->foff;

    pte_attr_t prot = region_pteprot(vmr);
    pte = pte_setprot(pte, prot);

    int errno = file->ops->read_page(file->inode, (void*)fault_va, mfile_off);
    if (errno < 0) {
        ERROR("fail to populate page (%d)", errno);
        return;
    }

    fault_resolved(fault, pte, 0);
}

static void
fault_handle_kernel_page(struct fault_context* fault)
{
    // TODO add check on faulting pointer
    //      we must ensure only ptep fault is resolvable
    fault_resolved(fault, pte_mkroot(fault->resolving), 0);
    pmm_set_attr(fault->prealloc_pa, PP_FGPERSIST);
}


static void
fault_prealloc_page(struct fault_context* fault)
{
    if (!pte_isnull(fault->fault_pte)) {
        return;
    }

    pte_t pte = mkpte(0, KERNEL_DATA);
    
    pte = vmm_alloc_page(fault->fault_ptep, pte);
    if (pte_isnull(pte)) {
        return;
    }

    fault->resolving = pte;
    fault->prealloc_pa = pte_paddr(fault->resolving);

    pmm_set_attr(fault->prealloc_pa, 0);
}


static void noret
__fail_to_resolve(struct fault_context* fault)
{
    if (fault->prealloc_pa) {
        pmm_free_page(fault->prealloc_pa);
    }

    ERROR("(pid: %d) Segmentation fault on %p (%p,e=0x%x)",
          __current->pid,
          fault->fault_va,
          fault->fault_instn,
          fault->fault_data);

    trace_printstack_isr(fault->ictx);

    if (fault->kernel_fault) {
        FATAL("[page fault on kernel]");
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
    if (guardian_page(fault_pte)) {
        ERROR("memory region over-running");
        return false;
    }

    if (fault->kernel_fault) {
        fault_handle_kernel_page(fault);
        goto done;
    }

    if (fault->vmr) {
        return false;
    }

    if (pte_isloaded(fault_pte)) {
        fault_handle_conflict_pte(fault);
    }
    else if (anon_region(fault->vmr)) {
        fault_handle_anon_region(fault);
    }
    else if (fault->vmr->mfile) {
        fault_handle_named_region(fault);
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

    if (!arch_fault_get_context(&fault)) {
        __fail_to_resolve(&fault);
    }

    fault_prealloc_page(&fault);

    if (!__try_resolve_fault(&fault)) {
        __fail_to_resolve(&fault);
    }

    if ((fault.resolve_type & NO_PREALLOC)) {
        if (fault.prealloc_pa) {
            pmm_free_page(fault.prealloc_pa);
        }
    }

    set_pte(fault.fault_ptep, fault.resolving);

    cpu_flush_page(fault.fault_va);
    cpu_flush_page((ptr_t)fault.fault_ptep);
}