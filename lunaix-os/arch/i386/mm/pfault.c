#include <lunaix/mm/mm.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/region.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/sched.h>
#include <lunaix/signal.h>
#include <lunaix/status.h>
#include <lunaix/syslog.h>
#include <lunaix/trace.h>

#include <sys/interrupts.h>
#include <sys/mm/mm_defs.h>

#include <klibc/string.h>

LOG_MODULE("pf")



#define COW_MASK (REGION_RSHARED | REGION_READ | REGION_WRITE)

extern void
__print_panic_msg(const char* msg, const isr_param* param);

void
intr_routine_page_fault(const isr_param* param)
{
    if (param->depth > 10) {
        // Too many nested fault! we must messed up something
        // XXX should we failed silently?
        spin();
    }

    uint32_t errcode = param->execp->err_code;
    ptr_t ptr = cpu_ldeaddr();
    if (!ptr) {
        goto segv_term;
    }

    v_mapping mapping;
    if (!vmm_lookup(ptr, &mapping)) {
        goto segv_term;
    }

    // XXX do kernel trigger pfault?

    // FIXME use latest vmm api

    pte_t* fault_ptep   = mkptep_va(VMS_SELF, ptr);
    pte_t fault_pte     = *fault_ptep;
    ptr_t fault_pa      = pte_paddr(fault_pte);
    pte_t resolved_pte  = fault_pte;

    if (guardian_page(fault_pte)) {
        ERROR("memory region over-running");
        goto segv_term;
    }
    
    vm_regions_t* vmr = vmregions(__current);
    struct mm_region* hit_region = region_get(vmr, ptr);

    if (!hit_region) {
        // 当你凝视深渊时……
        goto segv_term;
    }

    if (pte_isloaded(fault_pte)) {
        if (!pte_allow_user(fault_pte)) {
            // invalid access
            DEBUG("invalid user access. (%p->%p, attr:0x%x)",
                  mapping.va,
                  mapping.pa,
                  mapping.flags);
            goto segv_term;
        }

        assert(pte_iswprotect(fault_pte));

        if (writable_region(hit_region)) {
            // normal page fault, do COW
            ptr_t pa = (ptr_t)vmm_dup_page(fault_pa);

            pmm_free_page(fault_pa);
            resolved_pte = pte_setpaddr(fault_pte, pa);
            resolved_pte = pte_mkwritable(resolved_pte);

            goto resolved;
        }
        
        // impossible cases or accessing privileged page
        goto segv_term;
    }

    // an anonymous page and not present
    //   -> a new page need to be alloc
    if (anon_region(hit_region)) {
        if (!pte_isloaded(fault_pte)) {
            ptr_t pa = pmm_alloc_page(0);
            if (!pa) {
                goto oom;
            }

            pte_attr_t prot = region_pteprot(hit_region);
            resolved_pte = mkpte(pa, prot);

            // FIXME zeroing page should be handled in a common way
            memset((void*)PG_ALIGN(ptr), 0, PG_SIZE);
            goto resolved;
        }

        // permission denied on anon page (e.g., write on readonly page)
        goto segv_term;
    }

    // if mfile is set (Non-anonymous), then it is a mem map
    if (hit_region->mfile && !pte_isloaded(fault_pte)) {
        struct v_file* file = hit_region->mfile;

        ptr = PG_ALIGN(ptr);

        u32_t mseg_off = (ptr - hit_region->start);
        u32_t mfile_off = mseg_off + hit_region->foff;
        ptr_t pa = pmm_alloc_page(0);

        if (!pa) {
            goto oom;
        }

        pte_attr_t prot = region_pteprot(hit_region);
        resolved_pte = mkpte(pa, prot);

        // FIXME zeroing page should be handled in a common way
        memset((void*)ptr, 0, PG_SIZE);

        int errno = file->ops->read_page(file->inode, (void*)ptr, mfile_off);

        if (errno < 0) {
            ERROR("fail to populate page (%d)", errno);
            goto segv_term;
        }

        goto resolved;
    }

    // page not present, might be a chance to introduce swap file?
    __print_panic_msg("WIP page fault route", param);
    while (1)
        ;

oom:
    ERROR("out of memory");

segv_term:
    ERROR("(pid: %d) Segmentation fault on %p (%p:%p,e=0x%x)",
          __current->pid,
          ptr,
          param->execp->cs,
          param->execp->eip,
          param->execp->err_code);

    trace_printstack_isr(param);

    if (kernel_context(param)) {
        ERROR("[page fault on kernel]");
        // halt kernel if segv comes from kernel space
        spin();
    }

    thread_setsignal(current_thread, _SIGSEGV);

    schedule();
    // should not reach
    while (1)
        ;

resolved:
    pte_write_entry(fault_ptep, resolved_pte);

    cpu_flush_page(ptr);
    cpu_flush_page((ptr_t)fault_ptep);
    return;
}