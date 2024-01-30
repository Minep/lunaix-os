#include <lunaix/mm/region.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syscall.h>
#include <lunaix/syslog.h>
#include <lunaix/signal.h>

#include <sys/abi.h>
#include <sys/mm/mm_defs.h>

LOG_MODULE("FORK")

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

static inline void
__dup_fdtable(struct proc_info* pcb)
{
    for (size_t i = 0; i < VFS_MAX_FD; i++) {
        struct v_fd* fd = __current->fdtable->fds[i];
        if (!fd)
            continue;
        vfs_dup_fd(fd, &pcb->fdtable->fds[i]);
    }
}


static void
__dup_kernel_stack(struct thread* thread, ptr_t vm_mnt)
{
    ptr_t kstack_pn = PN(current_thread->kstack);

    // copy the kernel stack
    for (size_t i = 0; i <= PN(KSTACK_SIZE); i++) {
        volatile x86_pte_t* orig_ppte = &PTE_MOUNTED(VMS_SELF, i + kstack_pn);
        x86_pte_t p = *orig_ppte;
        ptr_t ppa = vmm_dup_page(PG_ENTRY_ADDR(p));
        pmm_free_page(PG_ENTRY_ADDR(p));
        
        ptr_t kstack = (i + kstack_pn) * PG_SIZE;
        vmm_set_mapping(vm_mnt, kstack, ppa, p & 0xfff, 0);
    }
}

struct thread*
dup_active_thread(struct proc_info* duped_pcb) 
{
    struct thread* th = alloc_thread(duped_pcb);

    th->intr_ctx = current_thread->intr_ctx;
    th->kstack = current_thread->kstack;
    th->ustack = current_thread->ustack;

    signal_dup_active_context(&th->sigctx);

    /*
     *  store the return value for forked process.
     *  this will be implicit carried over after kernel stack is copied.
     */
    store_retval_to(th, 0);

    return th;
}

pid_t
dup_proc()
{
    struct proc_info* pcb = alloc_process();
    struct thread* main_thread = dup_active_thread(pcb);
    
    pcb->parent = __current;

    if (__current->cwd) {
        pcb->cwd = __current->cwd;
        vfs_ref_dnode(pcb->cwd);
    }

    __dup_fdtable(pcb);
    procvm_dup(pcb);

    vmm_mount_pd(VMS_MOUNT_1, vmroot(pcb));

    __dup_kernel_stack(main_thread, VMS_MOUNT_1);

    // 根据 mm_region 进一步配置页表
    ptr_t copied_kstack = main_thread->ustack;
    struct mm_region *pos, *n;
    llist_for_each(pos, n, &pcb->mm->regions, head)
    {
        // 如果写共享，则不作处理。
        if ((pos->attr & REGION_WSHARED)) {
            continue;
        }

        // remove stack of other threads.
        if (stack_region(pos) && !region_contains(pos, copied_kstack)) {
            mem_unmap_region(VMS_MOUNT_1, pos);
            continue;
        }

        ptr_t start_vpn = pos->start >> 12;
        ptr_t end_vpn = pos->end >> 12;
        __mark_region(start_vpn, end_vpn, pos->attr);
    }

    vmm_unmount_pd(VMS_MOUNT_1);

    commit_process(pcb);
    commit_thread(main_thread);

    return pcb->pid;
}


__DEFINE_LXSYSCALL(pid_t, fork)
{
    return dup_proc();
}
