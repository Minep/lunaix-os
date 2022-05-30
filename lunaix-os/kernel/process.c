#include <lunaix/process.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/clock.h>
#include <lunaix/syslog.h>
#include <lunaix/common.h>

LOG_MODULE("PROC")

void dup_proc() {
    pid_t pid = alloc_pid();

    void* ptd_pp = pmm_alloc_page(pid, PP_FGPERSIST);
    x86_page_table* ptd = vmm_fmap_page(pid, PG_MOUNT_1, ptd_pp, PG_PREM_RW);
    x86_page_table* pptd = (x86_page_table*) L1_BASE_VADDR;

    for (size_t i = 0; i < PG_MAX_ENTRIES - 1; i++)
    {
        x86_pte_t ptde = pptd->entry[i];
        if (!ptde || !(ptde & PG_PRESENT)) {
            ptd->entry[i] = ptde;
            continue;
        }
        
        x86_page_table* ppt = (x86_page_table*) L2_VADDR(i);
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
                void* ppa = vmm_dup_page(va);
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

    struct proc_info pcb = (struct proc_info) {
        .created = clock_systime(),
        .pid = pid,
        .mm = __current->mm,
        .page_table = ptd_pp,
        .intr_ctx = __current->intr_ctx,
        .parent_created = __current->created
    };

    // 正如同fork一样，返回两次。
    pcb.intr_ctx.registers.eax = 0;
    __current->intr_ctx.registers.eax = pid;

    push_process(&pcb);
    
}