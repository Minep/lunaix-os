#include <arch/x86/interrupts.h>
#include <lunaix/common.h>
#include <lunaix/mm/mm.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/region.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/sched.h>
#include <lunaix/status.h>
#include <lunaix/syslog.h>

static void
kprintf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    __kprintf("PFAULT", fmt, args);
    va_end(args);
}

extern void
__print_panic_msg(const char* msg, const isr_param* param);

extern void __kernel_heap_start;

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
        goto segv_term;
    }

    struct mm_region* hit_region = region_get(&__current->mm.regions, ptr);

    if (!hit_region) {
        // Into the void...
        goto segv_term;
    }

    x86_pte_t* pte = PTE_MOUNTED(PD_REFERENCED, ptr >> 12);
    if (*pte & PG_PRESENT) {
        if ((hit_region->attr & REGION_PERM_MASK) ==
            (REGION_RSHARED | REGION_READ)) {
            // normal page fault, do COW
            cpu_invplg(pte);
            uintptr_t pa =
              (uintptr_t)vmm_dup_page(__current->pid, PG_ENTRY_ADDR(*pte));
            pmm_free_page(__current->pid, *pte & ~0xFFF);
            *pte = (*pte & 0xFFF) | pa | PG_WRITE;
            return;
        }
        // impossible cases or accessing privileged page
        goto segv_term;
    }

    if (!(*pte)) {
        // Invalid location
        goto segv_term;
    }
    uintptr_t loc = *pte & ~0xfff;
    // a writable page, not present, pte attr is not null
    //   and no indication of cached page -> a new page need to be alloc
    if ((hit_region->attr & REGION_WRITE) && (*pte & 0xfff) && !loc) {
        cpu_invplg(pte);
        uintptr_t pa = pmm_alloc_page(__current->pid, 0);
        *pte = *pte | pa | PG_PRESENT;
        return;
    }
    // page not present, bring it from disk or somewhere else
    __print_panic_msg("WIP page fault route", param);
    while (1)
        ;

segv_term:
    kprintf(KERROR "(pid: %d) Segmentation fault on %p (%p:%p)\n",
            __current->pid,
            ptr,
            param->cs,
            param->eip);
    terminate_proc(LXSEGFAULT);
    // should not reach
}

int
do_kernel(v_mapping* mapping)
{
    uintptr_t addr = mapping->va;
    if (addr >= &__kernel_heap_start && addr < L2_BASE_VADDR) {
        // This is kernel heap page
        uintptr_t pa = pmm_alloc_page(KERNEL_PID, 0);
        *mapping->pte = (*mapping->pte & 0xfff) | pa | PG_PRESENT;
        cpu_invplg(mapping->pte);
        cpu_invplg(addr);
        goto done;
    }

    return 0;
done:
    return 1;
}