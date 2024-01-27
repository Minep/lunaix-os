#include <lunaix/mm/region.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syscall.h>
#include <lunaix/syslog.h>
#include <lunaix/signal.h>

#include <sys/abi.h>
#include <sys/mm/mempart.h>

LOG_MODULE("FORK")

static ptr_t
__dup_pagetable(pid_t pid, ptr_t mount_point)
{
    ptr_t ptd_pp = pmm_alloc_page(pid, PP_FGPERSIST);
    vmm_set_mapping(VMS_SELF, PG_MOUNT_1, ptd_pp, PG_PREM_RW, VMAP_NULL);

    x86_page_table* ptd = (x86_page_table*)PG_MOUNT_1;
    x86_page_table* pptd = (x86_page_table*)(mount_point | (0x3FF << 12));

    size_t kspace_l1inx = L1_INDEX(KERNEL_EXEC);

    for (size_t i = 0; i < PG_MAX_ENTRIES - 1; i++) {

        x86_pte_t ptde = pptd->entry[i];
        // 空或者是未在内存中的L1页表项直接照搬过去。
        // 内核地址空间直接共享过去。
        if (!ptde || i >= kspace_l1inx || !(ptde & PG_PRESENT)) {
            ptd->entry[i] = ptde;
            continue;
        }

        // 复制L2页表
        ptr_t pt_pp = pmm_alloc_page(pid, PP_FGPERSIST);
        vmm_set_mapping(VMS_SELF, PG_MOUNT_2, pt_pp, PG_PREM_RW, VMAP_NULL);

        x86_page_table* ppt = (x86_page_table*)(mount_point | (i << 12));
        x86_page_table* pt = (x86_page_table*)PG_MOUNT_2;

        for (size_t j = 0; j < PG_MAX_ENTRIES; j++) {
            x86_pte_t pte = ppt->entry[j];
            pmm_ref_page(pid, PG_ENTRY_ADDR(pte));
            pt->entry[j] = pte;
        }

        ptd->entry[i] = (ptr_t)pt_pp | PG_ENTRY_FLAGS(ptde);
    }

    ptd->entry[PG_MAX_ENTRIES - 1] = NEW_L1_ENTRY(T_SELF_REF_PERM, ptd_pp);

    return ptd_pp;
}

ptr_t
vmm_dup_vmspace(pid_t pid)
{
    return __dup_pagetable(pid, VMS_SELF);
}

static void
__mark_region(ptr_t start_vpn, ptr_t end_vpn, int attr)
{
    for (size_t i = start_vpn; i <= end_vpn; i++) {
        x86_pte_t* curproc = &PTE_MOUNTED(VMS_SELF, i);
        x86_pte_t* newproc = &PTE_MOUNTED(VMS_MOUNT_1, i);

        cpu_flush_page((ptr_t)newproc);

        if ((attr & REGION_MODE_MASK) == REGION_RSHARED) {
            // 如果读共享，则将两者的都标注为只读，那么任何写入都将会应用COW策略。
            cpu_flush_page((ptr_t)curproc);
            cpu_flush_page((ptr_t)(i << 12));

            *curproc = *curproc & ~PG_WRITE;
            *newproc = *newproc & ~PG_WRITE;
        } else {
            // 如果是私有页，则将该页从新进程中移除。
            *newproc = 0;
        }
    }
}

static void
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
    pcb->intr_ctx = __current->intr_ctx;
    pcb->parent = __current;

    if (__current->cwd) {
        pcb->cwd = __current->cwd;
        vfs_ref_dnode(pcb->cwd);
    }

    __copy_fdtable(pcb);
    region_copy_mm(&__current->mm, &pcb->mm);
    signal_dup_context(pcb->sigctx);

    /*
     *  store the return value for forked process.
     *  this will be implicit carried over after kernel stack is copied.
     */
    store_retval(0);

    copy_kernel_stack(pcb, VMS_SELF);

    // 根据 mm_region 进一步配置页表

    struct mm_region *pos, *n;
    llist_for_each(pos, n, &pcb->mm.regions, head)
    {
        // 如果写共享，则不作处理。
        if ((pos->attr & REGION_WSHARED)) {
            continue;
        }

        ptr_t start_vpn = pos->start >> 12;
        ptr_t end_vpn = pos->end >> 12;
        __mark_region(start_vpn, end_vpn, pos->attr);
    }

    vmm_unmount_pd(VMS_MOUNT_1);

    commit_process(pcb);

    return pcb->pid;
}

extern void __kexec_end;

void
copy_kernel_stack(struct proc_info* proc, ptr_t usedMnt)
{
    // copy the entire kernel page table
    pid_t pid = proc->pid;
    ptr_t pt_copy = __dup_pagetable(pid, usedMnt);

    vmm_mount_pd(VMS_MOUNT_1, pt_copy); // 将新进程的页表挂载到挂载点#2

    // copy the kernel stack
    for (size_t i = KERNEL_STACK >> 12; i <= KERNEL_STACK_END >> 12; i++) {
        volatile x86_pte_t* ppte = &PTE_MOUNTED(VMS_MOUNT_1, i);

        /*
            This is a fucking nightmare, the TLB caching keep the rewrite to PTE
           from updating. Even the Nightmare Moon the Evil is far less nasty
           than this. It took me hours of debugging to figure this out.

            In the name of Celestia our glorious goddess, I will fucking HATE
           the TLB for the rest of my LIFE!
        */
        cpu_flush_page((ptr_t)ppte);

        x86_pte_t p = *ppte;
        ptr_t ppa = vmm_dup_page(pid, PG_ENTRY_ADDR(p));
        pmm_free_page(pid, PG_ENTRY_ADDR(p));
        *ppte = (p & 0xfff) | ppa;
    }

    proc->page_table = pt_copy;
}

__DEFINE_LXSYSCALL(pid_t, fork)
{
    return dup_proc();
}
