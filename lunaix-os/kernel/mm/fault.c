#include <lunaix/mm/vastm.h>
#include <lunaix/mm/fault.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/region.h>
#include <lunaix/mm/vmtlb.h>
#include <lunaix/sched.h>
#include <lunaix/signal.h>
#include <lunaix/status.h>
#include <lunaix/syslog.h>
#include <lunaix/trace.h>
#include <lunaix/hart_state.h>
#include <lunaix/failsafe.h>

#include <asm/mm_defs.h>

#include <klibc/string.h>

LOG_MODULE("pf")

static void
__prepare_fault_context(struct fault_context* fault)
{
    pte_t* fault_ptep, victim_pte, to_resolved;
    ptroot_t fault_vms;
    ptr_t  fault_va        = fault->fault_va;
    bool   kernel_vmfault  = kernel_addr(fault_va);
    
    if (kernel_vmfault) {
        fault_vms = vastm_current_root();
    } else {
        fault_vms = vastm_procvm_root(fault->mm);
    }

    fault_ptep = vastm_walk_ptep(fault_vms, fault_va, RES_LFT);
    victim_pte = *fault_ptep;

    if (pte_isnull(victim_pte)) {
        if (kernel_vmfault)
            to_resolved = mkpte_prot(KERNEL_PAGE);
        else
            to_resolved = mkpte_prot(USER_PAGE);
    } 
    else {
        to_resolved = victim_pte;
    }
    
    fault->resolved.attr = to_resolved;
    fault->resolved.leaflet = NULL;

    fault->fault_pte = victim_pte;
    fault->fault_vms = fault_vms;
    fault->kernel_vmfault = kernel_vmfault;
    fault->kernel_access  = kernel_context(fault->hstate);
}

static inline void
__flush_staled(struct fault_context* fault)
{
    tlb_flush_mm_range(fault->mm, fault->fault_va, 
                        leaflet_nfold(fault->resolved.leaflet));
}

static inline void
__resolve_fault_ptes(struct fault_context* fault)
{
    struct leaflet* resolved_leaflet;
    pte_t* ptep, pte;
    ptr_t pa;
    int page_counts;
    
    resolved_leaflet = fault->resolved.leaflet;
    page_counts = leaflet_nfold(resolved_leaflet);
    
    pa = leaflet_addr(resolved_leaflet);
    pte = fault->resolved.attr;

    ptep = vastm_make_along(fault->fault_vms, fault->fault_va, 
                            leaflet_order(resolved_leaflet), pte);
    
    for (int i = 0; i < page_counts; i++) {
        pte = pte_setpaddr(pte, pa + i * PAGE_SIZE);
        set_pte(ptep + i, pte);
    }

    __flush_staled(fault);
}

static inline void
__discard_prealloc_leaflet(struct fault_context* fault)
{
    if (fault->prealloc)
        leaflet_return(fault->prealloc);
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

        leaflet_return(fault_leaflet);

        fault_resolved(fault, pte, duped_leaflet);
    }

    return;
}


static void
__handle_anon_region(struct fault_context* fault)
{
    pte_t pte;

    pte = fault->resolved.attr;
    pte = region_set_pte_attrs(fault->vmr, pte);
    
    // we use prealloced leaflet
    fault_resolved(fault, pte, fault->prealloc);
}


static void
__handle_named_region(struct fault_context* fault)
{
    int errno;
    struct mm_region *vmr;
    struct v_file *file;
    struct leaflet* leaflet;

    pte_t pte;
    ptr_t page_va;
    u32_t mseg_off, mfile_off;
    size_t mapped_len;

    errno = 0;
    vmr = fault->vmr;
    file = vmr->mfile;
    leaflet = fault->prealloc;
    pte = fault->resolved.attr;

    mseg_off  = (page_frame(fault->fault_va) - vmr->start);
    mfile_off = mseg_off + vmr->foff;
    mapped_len = vmr->flen;
    page_va = leaflet_va(leaflet);

    if (mseg_off < mapped_len) {
        mapped_len = MIN(mapped_len - mseg_off, PAGE_SIZE);
    } else {
        mapped_len = 0;
    }

    if (mapped_len == PAGE_SIZE) {
        errno = file->ops->read_page(
                    file->inode, (void*)page_va, mfile_off);
    } else {
        leaflet_wipe(leaflet);
        
        if (mapped_len) {
            errno = file->ops->read(
                        file->inode, (void*)page_va, mapped_len, mfile_off);
        }
    }

    if (errno < 0) {
        ERROR("fail to populate page (%d)", errno);
        leaflet_return(leaflet);
        return;
    }

    pte = region_set_pte_attrs(vmr, pte);
    fault_resolved(fault, pte, leaflet);
}

static void
__handle_kernel_page(struct fault_context* fault)
{
    // We shouldn't expect any fault from kernel 
    //  at this stage of development
    return;
}


static void
fault_prealloc_page(struct fault_context* fault)
{
    if (!pte_isnull(fault->fault_pte)) {
        return;
    }

    pte_t pte;
    
    struct leaflet* leaflet = alloc_leaflet(0);
    if (unlikely(!leaflet)) {
        return;
    }

    fault->prealloc = leaflet;
}


void noret
fault_resolving_failed(struct fault_context* fault)
{
    __discard_prealloc_leaflet(fault);

    ERROR("(pid: %d) Segmentation fault on %p (%p,e=0x%x)",
          __current->pid,
          fault->fault_va,
          fault->fault_instn,
          fault->fault_data);


    if (fault->kernel_access) {
        // if a page fault from kernel is not resolvable, then
        //  something must be went south
        FATAL("unresolvable page fault");
        failsafe_diagnostic();
    }

    trace_printstack_isr(fault->hstate);
    
    thread_setsignal(current_thread, _SIGSEGV);

    schedule();
    fail("Unexpected return from segfault");

    unreachable;
}

static bool
__try_resolve_fault(struct fault_context* fault)
{
    pte_t fault_pte;

    fault_pte = fault->fault_pte;

    if (pte_isguardian(fault_pte)) {
        ERROR("memory region over-running");
        return false;
    }

    if (fault->kernel_vmfault && fault->kernel_access) {
        __handle_kernel_page(fault);
        goto done;
    }

    assert(fault->mm);
    fault->vmr = region_get(&fault->mm->regions, fault->fault_va);

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
    return !!(fault->resolved.result & RESOLVE_OK);
}

bool
handle_page_fault(struct fault_context* fault)
{
    __prepare_fault_context(fault);

    fault_prealloc_page(fault);

    if (!__try_resolve_fault(fault)) {
        return false;
    }

    if ((fault->resolved.result & NO_PREALLOC)) {
        __discard_prealloc_leaflet(fault);
    }

    __resolve_fault_ptes(fault);
    return true;
}
