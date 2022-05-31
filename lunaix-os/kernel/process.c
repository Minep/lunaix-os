#include <lunaix/process.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/mm/region.h>
#include <lunaix/clock.h>
#include <lunaix/syslog.h>
#include <lunaix/common.h>
#include <lunaix/syscall.h>
#include <lunaix/spike.h>

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
            x86_pte_t pte = ppt->entry[j];
            pmm_ref_page(pid, pte & ~0xfff);
            pt->entry[j] = pte;
        }

        ptd->entry[i] = (uintptr_t)pt_pp | PG_PREM_RW;
    }
    
    ptd->entry[PG_MAX_ENTRIES - 1] = NEW_L1_ENTRY(T_SELF_REF_PERM, ptd_pp);

    return ptd_pp;
}

void* dup_pagetable(pid_t pid) {
    return __dup_pagetable(pid, PD_REFERENCED);
}

__DEFINE_LXSYSCALL(void, fork) {
    dup_proc();
}

__DEFINE_LXSYSCALL(pid_t, getpid) {
    return __current->pid;
}

__DEFINE_LXSYSCALL(pid_t, getppid) {
    return __current->parent->pid;
}

void dup_proc() {
    pid_t pid = alloc_pid();

    struct proc_info pcb = (struct proc_info) {
        .created = clock_systime(),
        .pid = pid,
        .mm = __current->mm,
        .intr_ctx = __current->intr_ctx,
        .parent = __current
    };

    setup_proc_mem(&pcb, PD_MOUNT_1);    //挂载点#1是当前进程的页表

    // 根据 mm_region 进一步配置页表
    if (__current->mm.regions) {
        struct mm_region *pos, *n;
        llist_for_each(pos, n, &__current->mm.regions->head, head) {
            region_add(&pcb, pos->start, pos->end, pos->attr);

            // 如果写共享，则不作处理。
            if ((pos->attr & REGION_WSHARED)) {
                continue;
            }

            uintptr_t start_vpn = PG_ALIGN(pos->start) >> 12;
            uintptr_t end_vpn = PG_ALIGN(pos->end) >> 12;
            for (size_t i = start_vpn; i < end_vpn; i++)
            {
                x86_pte_t *curproc = &((x86_page_table*)(PD_MOUNT_1 | ((i & 0xffc00) << 2)))->entry[i & 0x3ff];
                x86_pte_t *newproc = &((x86_page_table*)(PD_MOUNT_2 | ((i & 0xffc00) << 2)))->entry[i & 0x3ff];

                if (pos->attr == REGION_RSHARED) {
                    // 如果读共享，则将两者的都标注为只读，那么任何写入都将会应用COW策略。
                    *curproc = *curproc & ~PG_WRITE;
                    *newproc = *newproc & ~PG_WRITE;
                }
                else {
                    // 如果是私有页，则将该页从新进程中移除。
                    *newproc = 0;
                }
            }
        }
    }
    
    vmm_unmount_pd(PD_MOUNT_2);

    // 正如同fork，返回两次。
    pcb.intr_ctx.registers.eax = 0;
    __current->intr_ctx.registers.eax = pid;

    push_process(&pcb);
}

extern void __kernel_end;

void setup_proc_mem(struct proc_info* proc, uintptr_t usedMnt) {
    // copy the entire kernel page table
    pid_t pid = proc->pid;
    void* pt_copy = __dup_pagetable(pid, usedMnt);

    vmm_mount_pd(PD_MOUNT_2, pt_copy);   // 将新进程的页表挂载到挂载点#2

    // copy the kernel stack
    for (size_t i = KSTACK_START >> 12; i <= KSTACK_TOP >> 12; i++)
    {
        x86_pte_t *ppte = &((x86_page_table*)(PD_MOUNT_2 | ((i & 0xffc00) << 2)))->entry[i & 0x3ff];
        void* ppa = vmm_dup_page(pid, PG_ENTRY_ADDR(*ppte));
        *ppte = (*ppte & 0xfff) | (uintptr_t)ppa;
    }

    // 我们不需要分配内核的区域，因为所有的内核代码和数据段只能通过系统调用来访问，任何非法的访问
    // 都会导致eip落在区域外面，从而segmentation fault.
    
    // 定义用户栈区域，但是不分配实际的物理页。我们会在Page fault handler里面实现动态分配物理页的逻辑。（虚拟内存的好处！）
    // FIXME: 这里应该放到spawn_proc里面。
    // region_add(proc, USTACK_END, USTACK_SIZE, REGION_PRIVATE | REGION_RW);

    // 至于其他的区域我们暂时没有办法知道，因为那需要知道用户程序的信息。我们留到之后在处理。

    proc->page_table = pt_copy;
}