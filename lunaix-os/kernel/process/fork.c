#include <lunaix/mm/vmtlb.h>
#include "asm/mempart.h"
#include "asm/pagetable.h"
#include "lunaix/mm/vastm.h"
#include <lunaix/mm/region.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syscall.h>
#include <lunaix/syslog.h>
#include <lunaix/signal.h>
#include <lunaix/kpreempt.h>

#include <asm/abi.h>
#include <asm/mm_defs.h>

#include <klibc/string.h>

LOG_MODULE("FORK")

static void
__dup_kernel_stack(struct thread* thread)
{
    // FIXME (2026.DUP_KSTACK)  integrate into vmrcpy workflow
    // Kernel stacks should be a dedicated mm_region as user stacks.
    // It make sense if we can reuse the logic of vmrcpy 

    struct leaflet* leaflet;
    ptr_t kstack;
    struct proc_mm* mm;
    pte_t *dest, *src, p;
    int nr_pages;

    mm = vmspace(thread->process);

    kstack = (current_thread->kstack) - KERNEL_STACK_UNITSIZE;

    // procvm_dupvms() do not copy kernel stacks, get the structure ready
    dest = vastm_make_along(
            vastm_procvm_root(mm), kstack, RES_LFT, mkpte_prot(KERNEL_PGTAB));

    src = vastm_walk_ptep_strict(
            vastm_procvm_root(__current->mm), kstack, RES_LFT);

    assert(dest && src);

    for (size_t i = 0; i <= count_pages(KERNEL_STACK_UNITSIZE); ) 
    {
        p = pte_at(&src[i]);

        if (pte_isguardian(p)) {
            set_pte(&dest[i++], guard_pte);
            continue;
        }

        leaflet = dup_leaflet(pte_leaflet(p));
        nr_pages = leaflet_nfold(leaflet);

        set_ptes(&dest[i], p, leaflet_addr(leaflet), nr_pages);
        i += nr_pages;
    
    }

    tlb_flush_mm_range(
            mm, page_index(kstack), count_pages(KERNEL_STACK_UNITSIZE));
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
dup_active_thread(struct proc_info* duped_pcb) 
{
    struct thread* th = alloc_thread(duped_pcb);
    if (!th) {
        return NULL;
    }

    th->hstate = current_thread->hstate;
    th->kstack = current_thread->kstack;

    signal_dup_context(&th->sigctx);

    /*
     *  store the return value for forked process.
     *  this will be implicit carried over after kernel stack is copied.
     */
    store_retval_to(th, 0);

    __dup_kernel_stack(th);

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
            mem_unmap_region(pos);
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
    struct thread* main_thread;
    struct proc_info* pcb;
    
    no_preemption();
    
    pcb = alloc_process();
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

    fdtable_copy(pcb->fdtable, __current->fdtable);
    uscope_copy(&pcb->uscope, current_user_scope());

    procvm_dupvms(vmspace(pcb));

    main_thread = dup_active_thread(pcb);
    if (!main_thread) {
        syscall_result(ENOMEM);
        delete_process(pcb);
        return -1;
    }

    commit_process(pcb);
    commit_thread(main_thread);

    return pcb->pid;
}

__DEFINE_LXSYSCALL(pid_t, fork)
{
    return dup_proc();
}
