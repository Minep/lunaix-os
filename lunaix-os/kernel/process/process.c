#include <klibc/string.h>
#include <lunaix/clock.h>
#include <lunaix/common.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/region.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syscall.h>
#include <lunaix/syslog.h>

LOG_MODULE("PROC")

void*
__dup_pagetable(pid_t pid, uintptr_t mount_point)
{
    void* ptd_pp = pmm_alloc_page(pid, PP_FGPERSIST);
    vmm_set_mapping(PD_REFERENCED, PG_MOUNT_1, ptd_pp, PG_PREM_RW, VMAP_NULL);

    x86_page_table* ptd = PG_MOUNT_1;
    x86_page_table* pptd = (x86_page_table*)(mount_point | (0x3FF << 12));

    size_t kspace_l1inx = L1_INDEX(KERNEL_MM_BASE);

    for (size_t i = 0; i < PG_MAX_ENTRIES - 1; i++) {

        x86_pte_t ptde = pptd->entry[i];
        // 空或者是未在内存中的L1页表项直接照搬过去。
        // 内核地址空间直接共享过去。
        if (!ptde || i >= kspace_l1inx || !(ptde & PG_PRESENT)) {
            ptd->entry[i] = ptde;
            continue;
        }

        // 复制L2页表
        void* pt_pp = pmm_alloc_page(pid, PP_FGPERSIST);
        vmm_set_mapping(
          PD_REFERENCED, PG_MOUNT_2, pt_pp, PG_PREM_RW, VMAP_NULL);

        x86_page_table* ppt = (x86_page_table*)(mount_point | (i << 12));
        x86_page_table* pt = PG_MOUNT_2;

        for (size_t j = 0; j < PG_MAX_ENTRIES; j++) {
            x86_pte_t pte = ppt->entry[j];
            pmm_ref_page(pid, PG_ENTRY_ADDR(pte));
            pt->entry[j] = pte;
        }

        ptd->entry[i] = (uintptr_t)pt_pp | PG_ENTRY_FLAGS(ptde);
    }

    ptd->entry[PG_MAX_ENTRIES - 1] = NEW_L1_ENTRY(T_SELF_REF_PERM, ptd_pp);

    return ptd_pp;
}

void
__del_pagetable(pid_t pid, uintptr_t mount_point)
{
    x86_page_table* pptd = (x86_page_table*)(mount_point | (0x3FF << 12));

    // only remove user address space
    for (size_t i = 0; i < L1_INDEX(KERNEL_MM_BASE); i++) {
        x86_pte_t ptde = pptd->entry[i];
        if (!ptde || !(ptde & PG_PRESENT)) {
            continue;
        }

        x86_page_table* ppt = (x86_page_table*)(mount_point | (i << 12));

        for (size_t j = 0; j < PG_MAX_ENTRIES; j++) {
            x86_pte_t pte = ppt->entry[j];
            // free the 4KB data page
            if ((pte & PG_PRESENT)) {
                pmm_free_page(pid, PG_ENTRY_ADDR(pte));
            }
        }
        // free the L2 page table
        pmm_free_page(pid, PG_ENTRY_ADDR(ptde));
    }
    // free the L1 directory
    pmm_free_page(pid, PG_ENTRY_ADDR(pptd->entry[PG_MAX_ENTRIES - 1]));
}

void*
vmm_dup_vmspace(pid_t pid)
{
    return __dup_pagetable(pid, PD_REFERENCED);
}

__DEFINE_LXSYSCALL(pid_t, fork)
{
    return dup_proc();
}

__DEFINE_LXSYSCALL(pid_t, getpid)
{
    return __current->pid;
}

__DEFINE_LXSYSCALL(pid_t, getppid)
{
    return __current->parent->pid;
}

__DEFINE_LXSYSCALL(pid_t, getpgid)
{
    return __current->pgid;
}

__DEFINE_LXSYSCALL2(int, setpgid, pid_t, pid, pid_t, pgid)
{
    struct proc_info* proc = pid ? get_process(pid) : __current;

    if (!proc) {
        __current->k_status = EINVAL;
        return -1;
    }

    pgid = pgid ? pgid : proc->pid;

    struct proc_info* gruppenfuhrer = get_process(pgid);

    if (!gruppenfuhrer || proc->pgid == gruppenfuhrer->pid) {
        __current->k_status = EINVAL;
        return -1;
    }

    llist_delete(&proc->grp_member);
    llist_append(&gruppenfuhrer->grp_member, &proc->grp_member);

    proc->pgid = pgid;
    return 0;
}

void
init_proc_user_space(struct proc_info* pcb)
{
    vmm_mount_pd(PD_MOUNT_1, pcb->page_table);

    /*---  分配用户栈  ---*/

    // 注册用户栈区域
    region_add(
      &pcb->mm.regions, USTACK_END, USTACK_TOP, REGION_RW | REGION_RSHARED);

    // 预留地址空间，具体物理页将由Page Fault Handler按需分配。
    for (uintptr_t i = PG_ALIGN(USTACK_END); i < USTACK_TOP; i += PG_SIZE) {
        vmm_set_mapping(PD_MOUNT_1, i, 0, PG_ALLOW_USER | PG_WRITE, VMAP_NULL);
    }

    // TODO other uspace initialization stuff

    vmm_unmount_pd(PD_MOUNT_1);
}

void
__mark_region(uintptr_t start_vpn, uintptr_t end_vpn, int attr)
{
    for (size_t i = start_vpn; i <= end_vpn; i++) {
        x86_pte_t* curproc = &PTE_MOUNTED(PD_REFERENCED, i);
        x86_pte_t* newproc = &PTE_MOUNTED(PD_MOUNT_1, i);
        cpu_invplg(newproc);

        if ((attr & REGION_MODE_MASK) == REGION_RSHARED) {
            // 如果读共享，则将两者的都标注为只读，那么任何写入都将会应用COW策略。
            cpu_invplg(curproc);
            cpu_invplg(i << 12);
            *curproc = *curproc & ~PG_WRITE;
            *newproc = *newproc & ~PG_WRITE;
        } else {
            // 如果是私有页，则将该页从新进程中移除。
            *newproc = 0;
        }
    }
}

void
__copy_fdtable(struct proc_info* pcb)
{
    for (size_t i = 0; i < VFS_MAX_FD; i++) {
        struct v_fd* fd = __current->fdtable->fds[i];
        if (!fd)
            continue;
        vfs_dup_fd(fd, &pcb->fdtable->fds[i]);
    }
}

pid_t
dup_proc()
{
    struct proc_info* pcb = alloc_process();
    pcb->mm.u_heap = __current->mm.u_heap;
    pcb->intr_ctx = __current->intr_ctx;
    pcb->parent = __current;

    memcpy(pcb->fxstate, __current->fxstate, 512);

    if (__current->cwd) {
        pcb->cwd = __current->cwd;
        vfs_ref_dnode(pcb->cwd);
    }

    __copy_fdtable(pcb);
    region_copy(&__current->mm.regions, &pcb->mm.regions);

    setup_proc_mem(pcb, PD_REFERENCED);

    // 根据 mm_region 进一步配置页表

    struct mm_region *pos, *n;
    llist_for_each(pos, n, &pcb->mm.regions.head, head)
    {
        // 如果写共享，则不作处理。
        if ((pos->attr & REGION_WSHARED)) {
            continue;
        }

        uintptr_t start_vpn = pos->start >> 12;
        uintptr_t end_vpn = pos->end >> 12;
        __mark_region(start_vpn, end_vpn, pos->attr);
    }

    vmm_unmount_pd(PD_MOUNT_1);

    // 正如同fork，返回两次。
    pcb->intr_ctx.registers.eax = 0;

    commit_process(pcb);

    return pcb->pid;
}

extern void __kernel_end;

void
setup_proc_mem(struct proc_info* proc, uintptr_t usedMnt)
{
    // copy the entire kernel page table
    pid_t pid = proc->pid;
    void* pt_copy = __dup_pagetable(pid, usedMnt);

    vmm_mount_pd(PD_MOUNT_1, pt_copy); // 将新进程的页表挂载到挂载点#2

    // copy the kernel stack
    for (size_t i = KSTACK_START >> 12; i <= KSTACK_TOP >> 12; i++) {
        volatile x86_pte_t* ppte = &PTE_MOUNTED(PD_MOUNT_1, i);

        /*
            This is a fucking nightmare, the TLB caching keep the rewrite to PTE
           from updating. Even the Nightmare Moon the Evil is far less nasty
           than this. It took me hours of debugging to figure this out.

            In the name of Celestia our glorious goddess, I will fucking HATE
           the TLB for the rest of my LIFE!
        */
        cpu_invplg(ppte);

        x86_pte_t p = *ppte;
        void* ppa = vmm_dup_page(pid, PG_ENTRY_ADDR(p));
        pmm_free_page(pid, PG_ENTRY_ADDR(p));
        *ppte = (p & 0xfff) | (uintptr_t)ppa;
    }

    // 我们不需要分配内核的区域，因为所有的内核代码和数据段只能通过系统调用来访问，任何非法的访问
    // 都会导致eip落在区域外面，从而segmentation fault.

    // 至于其他的区域我们暂时没有办法知道，因为那需要知道用户程序的信息。我们留到之后在处理。
    proc->page_table = pt_copy;
}