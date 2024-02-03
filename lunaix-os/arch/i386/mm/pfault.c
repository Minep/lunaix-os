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

#include <klibc/string.h>

LOG_MODULE("pf")



#define COW_MASK (REGION_RSHARED | REGION_READ | REGION_WRITE)

extern void
__print_panic_msg(const char* msg, const isr_param* param);

void
intr_routine_page_fault(const isr_param* param)
{
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

    vm_regions_t* vmr = vmregions(__current);
    struct mm_region* hit_region = region_get(vmr, ptr);

    if (!hit_region) {
        // 当你凝视深渊时……
        goto segv_term;
    }

    volatile x86_pte_t* pte = &PTE_MOUNTED(VMS_SELF, ptr >> 12);
    if (PG_IS_PRESENT(*pte)) {
        if (((errcode ^ mapping.flags) & PG_ALLOW_USER)) {
            // invalid access
            DEBUG("invalid user access. (%p->%p, attr:0x%x)",
                  mapping.va,
                  mapping.pa,
                  mapping.flags);
            goto segv_term;
        }
        if ((hit_region->attr & COW_MASK) == COW_MASK) {
            // normal page fault, do COW
            cpu_flush_page((ptr_t)pte);

            ptr_t pa = (ptr_t)vmm_dup_page(PG_ENTRY_ADDR(*pte));

            pmm_free_page(*pte & ~0xFFF);
            *pte = (*pte & 0xFFF & ~PG_DIRTY) | pa | PG_WRITE;

            goto resolved;
        }
        // impossible cases or accessing privileged page
        goto segv_term;
    }

    // an anonymous page and not present
    //   -> a new page need to be alloc
    if ((hit_region->attr & REGION_ANON)) {
        if (!PG_IS_PRESENT(*pte)) {
            cpu_flush_page((ptr_t)pte);

            ptr_t pa = pmm_alloc_page(0);
            if (!pa) {
                goto oom;
            }

            *pte = pa | region_ptattr(hit_region);
            memset((void*)PG_ALIGN(ptr), 0, PG_SIZE);
            goto resolved;
        }
        // permission denied on anon page (e.g., write on readonly page)
        goto segv_term;
    }

    // if mfile is set (Non-anonymous), then it is a mem map
    if (hit_region->mfile && !PG_IS_PRESENT(*pte)) {
        struct v_file* file = hit_region->mfile;

        ptr = PG_ALIGN(ptr);

        u32_t mseg_off = (ptr - hit_region->start);
        u32_t mfile_off = mseg_off + hit_region->foff;
        ptr_t pa = pmm_alloc_page(0);

        if (!pa) {
            goto oom;
        }

        cpu_flush_page((ptr_t)pte);
        *pte = pa | region_ptattr(hit_region);

        memset((void*)ptr, 0, PG_SIZE);

        int errno = file->ops->read_page(file->inode, (void*)ptr, mfile_off);

        if (errno < 0) {
            ERROR("fail to populate page (%d)", errno);
            goto segv_term;
        }

        *pte &= ~PG_DIRTY;

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
    cpu_flush_page(ptr);
    return;
}