#include <lunaix/process.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/clock.h>
#include <lunaix/syslog.h>
#include <lunaix/common.h>

LOG_MODULE("PROC")

void* __dup_pagetable(pid_t pid, uintptr_t mount_point) {
    void* ptd_pp = pmm_alloc_page(pid, PP_FGPERSIST);
    x86_page_table* ptd = vmm_fmap_page(pid, PG_MOUNT_1, ptd_pp, PG_PREM_RW);
    x86_page_table* pptd = (x86_page_table*) (mount_point | (0x3FF << 12));

    for (size_t i = 0; i < PG_MAX_ENTRIES - 1; i++)
    {
        x86_pte_t ptde = pptd->entry[i];
        if (!ptde || !(ptde & PG_PRESENT)) {
            ptd->entry[i] = ptde;
            continue;
        }
        
        x86_page_table* ppt = (x86_page_table*) (mount_point | (i << 12));
        void* pt_pp = pmm_alloc_page(pid, PP_FGPERSIST);
        x86_page_table* pt = vmm_fmap_page(pid, PG_MOUNT_2, pt_pp, PG_PREM_RW);

        for (size_t j = 0; j < PG_MAX_ENTRIES; j++)
        {
            uintptr_t va = ((i << 10) | j) << 12;
            x86_pte_t ppte = ppt->entry[j];
            if (!ppte || !(ppte & PG_PRESENT)) {
                pt->entry[j] = ppte;
                continue;
            }

            // FIXME: 根据 mm_region 将读共享的页（如堆）标为只读，而私有的页（如栈），则复制；而写共享的页则无需更改flags
            if (va >= KSTACK_START) {
                void* ppa = vmm_dup_page(pid, PG_ENTRY_ADDR(ppte));
                ppte = ppte & 0xfff | (uintptr_t)ppa;
            }
            pt->entry[j] = ppte;
            // ppte = ppte & ~PG_WRITE;
            // pt->entry[j] = ppte;
            // ppt->entry[j] = ppte;
        }

        ptd->entry[i] = (uintptr_t)pt_pp | PG_PREM_RW;
    }
    
    ptd->entry[PG_MAX_ENTRIES - 1] = NEW_L1_ENTRY(T_SELF_REF_PERM, ptd_pp);

    return ptd_pp;
}

void* dup_pagetable(pid_t pid) {
    return __dup_pagetable(pid, L2_BASE_VADDR);
}

void dup_proc() {
    pid_t pid = alloc_pid();

    /* 
        FIXME: Problematic! It should mount the page table of process then copy it.
        The current implementation copy the CURRENTLY loaded pgt.
        However, dup_pagetable is designed to copy current loaded pgt.
        
    */

    void* mnt_pt = vmm_mount_pd(__current->page_table);

    void* pg = __dup_pagetable(pid, mnt_pt);

    vmm_unmount_pd();

    struct proc_info pcb = (struct proc_info) {
        .created = clock_systime(),
        .pid = pid,
        .mm = __current->mm,
        .page_table = pg,
        .intr_ctx = __current->intr_ctx,
        .parent_created = __current->created
    };

    // 正如同fork，返回两次。
    pcb.intr_ctx.registers.eax = 0;
    __current->intr_ctx.registers.eax = pid;

    push_process(&pcb);
    
}