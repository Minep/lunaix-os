#include <arch/x86/interrupts.h>
#include <lunaix/common.h>
#include <lunaix/mm/mm.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/region.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/sched.h>
#include <lunaix/signal.h>
#include <lunaix/status.h>
#include <lunaix/syslog.h>

#include <klibc/string.h>

static void
kprintf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    __kprintf("PFAULT", fmt, args);
    va_end(args);
}

#define COW_MASK (REGION_RSHARED | REGION_READ | REGION_WRITE)

extern void
__print_panic_msg(const char* msg, const isr_param* param);

void
intr_routine_page_fault(const isr_param* param)
{
    uintptr_t ptr = cpu_rcr2();
    if (!ptr) {
        goto segv_term;
    }

    v_mapping mapping;
    if (!vmm_lookup(ptr, &mapping)) {
        goto segv_term;
    }

    if (!SEL_RPL(param->cs)) {
        // 如果是内核页错误……
        if (do_kernel(&mapping)) {
            return;
        }
        // 如果不是，那么看看内核是不是需要用户页。
    }

    struct mm_region* hit_region = region_get(&__current->mm.regions, ptr);

    if (!hit_region) {
        // 当你凝视深渊时……
        goto segv_term;
    }

    volatile x86_pte_t* pte = &PTE_MOUNTED(VMS_SELF, ptr >> 12);
    if (PG_IS_PRESENT(*pte)) {
        if ((hit_region->attr & COW_MASK) == COW_MASK) {
            // normal page fault, do COW
            cpu_invplg(pte);
            uintptr_t pa =
              (uintptr_t)vmm_dup_page(__current->pid, PG_ENTRY_ADDR(*pte));
            pmm_free_page(__current->pid, *pte & ~0xFFF);
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
            cpu_invplg(pte);
            uintptr_t pa = pmm_alloc_page(__current->pid, 0);
            if (!pa) {
                goto oom;
            }

            *pte = *pte | pa | PG_PRESENT;
            goto resolved;
        }
        // permission denied on anon page (e.g., write on readonly page)
        goto segv_term;
    }

    // if mfile is set (Non-anonymous), then it is a mem map
    if (hit_region->mfile && !PG_IS_PRESENT(*pte)) {
        struct v_file* file = hit_region->mfile;
        u32_t offset =
          (ptr - hit_region->start) & (PG_SIZE - 1) + hit_region->foff;
        uintptr_t pa = pmm_alloc_page(__current->pid, 0);

        if (!pa) {
            goto oom;
        }

        cpu_invplg(pte);
        *pte = (*pte & 0xFFF) | pa | PG_PRESENT;

        ptr = PG_ALIGN(ptr);
        memset(ptr, 0, PG_SIZE);

        int errno = 0;
        if (hit_region->init_page) {
            errno = hit_region->init_page(hit_region, ptr, offset);
        } else {
            errno = file->ops->read_page(file->inode, ptr, PG_SIZE, offset);
        }

        if (errno < 0) {
            kprintf(KERROR "fail to populate page (%d)\n", errno);
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
    kprintf(KERROR "out of memory\n");
segv_term:
    kprintf(KERROR "(pid: %d) Segmentation fault on %p (%p:%p)\n",
            __current->pid,
            ptr,
            param->cs,
            param->eip);
    __SIGSET(__current->sig_pending, _SIGSEGV);
    schedule();
    // should not reach
    while (1)
        ;

resolved:
    cpu_invplg(ptr);
    return;
}

int
do_kernel(v_mapping* mapping)
{
    uintptr_t addr = mapping->va;

    // TODO

    return 0;
done:
    return 1;
}