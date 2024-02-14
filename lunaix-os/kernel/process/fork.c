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

#include <klibc/string.h>

LOG_MODULE("FORK")

static void
region_maybe_cow(struct mm_region* region)
{
    int attr = region->attr;
    if ((attr & REGION_WSHARED)) {
        return;
    }

    pfn_t start_pn = pfn(region->start);
    pfn_t end_pn = pfn(region->end);
    
    for (size_t i = start_pn; i <= end_pn; i++) {
        pte_t* self = mkptep_pn(VMS_SELF, i);
        pte_t* guest = mkptep_pn(VMS_MOUNT_1, i);

        cpu_flush_page(page_addr(ptep_pfn(self)));

        if ((attr & REGION_MODE_MASK) == REGION_RSHARED) {
            set_pte(self, pte_mkwprotect(*self));
            set_pte(guest, pte_mkwprotect(*guest));
        } else {
            // 如果是私有页，则将该页从新进程中移除。
            set_pte(guest, null_pte);
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
    ptr_t kstack_pn = pfn(current_thread->kstack);

    // copy the kernel stack
    pte_t* src_ptep = mkptep_pn(VMS_SELF, kstack_pn);
    pte_t* dest_ptep = mkptep_pn(vm_mnt, kstack_pn);
    for (size_t i = 0; i < pfn(KSTACK_SIZE); i++) {
        pte_t p = *src_ptep;

        if (pte_isguardian(p)) {
            set_pte(dest_ptep, guard_pte);
        } else {
            ptr_t ppa = vmm_dup_page(pte_paddr(p));
            set_pte(dest_ptep, pte_setpaddr(p, ppa));
        }

        src_ptep++;
        dest_ptep++;
    }
}

/*
    Duplicate the current active thread to the forked process's
    main thread.

    This is not the same as "fork a thread within the same 
    process". In fact, it is impossible to do such "thread forking"
    as the new forked thread's kernel and user stack must not
    coincide with the original thread (because the same vm space)
    thus all reference to the stack space are staled which could 
    lead to undefined behaviour.

*/

static struct thread*
dup_active_thread(ptr_t vm_mnt, struct proc_info* duped_pcb) 
{
    struct thread* th = alloc_thread(duped_pcb);
    if (!th) {
        return NULL;
    }

    th->intr_ctx = current_thread->intr_ctx;
    th->kstack = current_thread->kstack;

    signal_dup_context(&th->sigctx);

    /*
     *  store the return value for forked process.
     *  this will be implicit carried over after kernel stack is copied.
     */
    store_retval_to(th, 0);

    __dup_kernel_stack(th, vm_mnt);

    if (!current_thread->ustack) {
        goto done;
    }

    struct mm_region* old_stack = current_thread->ustack;
    struct mm_region *pos, *n;
    llist_for_each(pos, n, vmregions(duped_pcb), head)
    {
        // remove stack of other threads.
        if (!stack_region(pos)) {
            continue;
        }

        if (!same_region(pos, old_stack)) {
            mem_unmap_region(vm_mnt, pos);
        }
        else {
            th->ustack = pos;
        }
    }

    assert(th->ustack);

done:
    return th;
}

pid_t
dup_proc()
{
    struct proc_info* pcb = alloc_process();
    if (!pcb) {
        syscall_result(ENOMEM);
        return -1;
    }
    
    pcb->parent = __current;

    // FIXME need a more elagent refactoring
    if (__current->cmd) {
        pcb->cmd_len = __current->cmd_len;
        pcb->cmd = valloc(pcb->cmd_len);
        memcpy(pcb->cmd, __current->cmd, pcb->cmd_len);
    }

    if (__current->cwd) {
        pcb->cwd = __current->cwd;
        vfs_ref_dnode(pcb->cwd);
    }

    __dup_fdtable(pcb);

    procvm_dup_and_mount(VMS_MOUNT_1, pcb);

    struct thread* main_thread = dup_active_thread(VMS_MOUNT_1, pcb);
    if (!main_thread) {
        syscall_result(ENOMEM);
        vms_unmount(VMS_MOUNT_1);
        delete_process(pcb);
        return -1;
    }

    // 根据 mm_region 进一步配置页表
    struct mm_region *pos, *n;
    llist_for_each(pos, n, &pcb->mm->regions, head)
    {
        region_maybe_cow(pos);
    }

    vms_unmount(VMS_MOUNT_1);

    commit_process(pcb);
    commit_thread(main_thread);

    return pcb->pid;
}

__DEFINE_LXSYSCALL(pid_t, fork)
{
    return dup_proc();
}
