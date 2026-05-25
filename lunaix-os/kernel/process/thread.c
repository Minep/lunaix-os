#include <lunaix/mm/pagetable.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/page.h>
#include <lunaix/syslog.h>
#include <lunaix/kpreempt.h>
#include <lunaix/mm/vastm.h>

#include <usr/lunaix/threads.h>

#include <asm/abi.h>
#include <asm/mm_defs.h>

LOG_MODULE("THREAD")

static ptr_t
__alloc_user_thread_stack(struct proc_info* proc, 
                          struct mm_region** stack_region)
{
    struct mm_region* vmr;
    struct mmap_param param;
    ptr_t th_stack_top;
    pte_t *guardp;
    int errno;

    th_stack_top = (proc->thread_count + 1) * USER_STACK_UNITSIZE;
    th_stack_top = mempart_end(USER_STACK_ZONE) - th_stack_top;
    th_stack_top = ROUNDUP(th_stack_top, PAGE_SIZE);

    param = (struct mmap_param) { 
              .pvms = vmspace(proc),
              .mlen = USER_STACK_UNITSIZE,
              .proct = PROT_READ | PROT_WRITE,
              .flags = MAP_ANON | MAP_PRIVATE,
              .type = REGION_TYPE_STACK 
    };

    errno = mmap_user((void**)&th_stack_top, &vmr, th_stack_top, NULL, &param);
    if (errno) {
        WARN("failed to create user thread stack: %d", errno);
        return 0;
    }

    guardp = vastm_make_along(vastm_procvm_root(param.pvms), 
                              vmr->start, RES_LFT, mkpte_prot(USER_DATA));
    set_pte(guardp, guard_pte);

    *stack_region = vmr;

    return align_stack(th_stack_top + USR_STACK_SIZE_THREAD - 1);
}

struct __kstack_alloc
{
    pte_t* ptep;
    ptr_t va;
};

static enum vastm_action
__alloc_kstack_mkinterim(struct vastm_state *state, pte_t *entry, void* data)
{
    if (pte_isnull(pte_at(entry))) {
        vastm_helper_create_intrim_table(state, entry, KERNEL_PGTAB);
    } 

    vastm_visit_next(*state, ptep_next_table(entry));
    return VASTM_CONTINUE;
}

static enum vastm_action
__alloc_kstack_find_slot(struct vastm_state *state, pte_t *entry, void* data)
{
    int i;
    int nr_pages;
    struct __kstack_alloc *result;
    
    result = (struct __kstack_alloc*)data;
    i = ptep_entry_index(entry);
    // plus 1 to account for guardian
    nr_pages = KERNEL_STACK_UNITSIZE / PAGE_SIZE + 1;

    while (state->va < state->va_end && i + nr_pages < LFT_LENGTH)
    {
        if (pte_isnull(pte_at(&entry[i]))) {
            result->ptep = &entry[i];
            result->va = state->va;
            return vastm_walk_flag_complete(state);
        }

        i += nr_pages;
        state->va += nr_pages * PAGE_SIZE;
    }

    return VASTM_BREAK;
}

static ptr_t
__alloc_kernel_thread_stack(struct proc_info* proc)
{
    /**
     *  FIXME (2026-KMEM_REGION) kthread stack refactor
     *
     *  The creation and deletion of kthread stack should 
     *  be part of a specical proc_vm - `kproc_vm`, thus
     *  it can follow the same route of mem_map
     */

    struct __kstack_alloc result;
    struct vastm param;

    result.ptep = NULL;
    vastm_param_prepare(&param, &result);
    vastm_param_cb_set_interims(&param, __alloc_kstack_mkinterim);
    vastm_param_cb_set(&param, ASTM_LFT, __alloc_kstack_find_slot);

    vastm_walk(
            &param, vastm_procvm_root(proc->mm), 
            mempart(KERNEL_STACK_ZONE), mempart_end(KERNEL_STACK_ZONE), 
            RES_LFT);
    
    if (!result.ptep) {
        WARN("failed to create kernel stack: max stack num reach\n");
        return 0;
    }

    unsigned int po = count_order(KERNEL_STACK_UNITSIZE / PAGE_SIZE);
    struct leaflet* leaflet = leaflet_alloc_order(PGPOL_NORMAL, po);

    if (!leaflet) {
        WARN("failed to create kernel stack: nomem\n");
        return 0;
    }

    set_pte(result.ptep++, guard_pte);
    set_ptes(result.ptep, mkpte_prot(KERNEL_DATA), 
            leaflet_addr(leaflet), 1 << po);

    return align_stack(result.va + KERNEL_STACK_UNITSIZE + PAGE_SIZE - 1);
}

void
thread_release_mem(struct thread* thread)
{
    pte_t *ptep;
    struct leaflet* leaflet;
    struct proc_mm* mm = vmspace(thread->process);

    ptep = vastm_walk_ptep_strict(
            vastm_procvm_root(mm), thread->kstack, RES_LFT);

    assert(ptep);
    leaflet = pte_leaflet(pte_at(ptep));
    
    ptep -= leaflet_nfold(leaflet);
    fill_ptes(ptep, null_pte, leaflet_nfold(leaflet));

    leaflet_return(leaflet);
    
    if (!thread->ustack)
        return;

    if ((thread->ustack->start & 0xfff))
        fail("invalid ustack struct");

    mem_unmap_region(thread->ustack);
}

struct thread*
create_thread(struct proc_info* proc, bool with_ustack)
{
    struct proc_mm* mm = vmspace(proc);

    struct mm_region* ustack_region = NULL;
    if (with_ustack && 
        !(__alloc_user_thread_stack(proc, &ustack_region))) 
    {
        return NULL;
    }

    ptr_t kstack = __alloc_kernel_thread_stack(proc);
    if (!kstack) {
        mem_unmap_region(ustack_region);
        return NULL;
    }

    struct thread* th = alloc_thread(proc);
    if (!th) {
        return NULL;
    }
    
    th->kstack = kstack;
    th->ustack = ustack_region;
    
    if (ustack_region) {
        th->ustack_top = align_stack(ustack_region->end - 1);
    }

    return th;
}

void
start_thread(struct thread* th, ptr_t entry)
{
    struct hart_transition transition;
    
    assert(th && entry);
    if (!kernel_addr(entry)) {
        assert(th->ustack);

        hart_user_transfer(&transition, th->kstack, th->ustack_top, entry);
    } 
    else {
        hart_kernel_transfer(&transition, th->kstack, entry);
    }

    install_hart_transition(vmspace(th->process), &transition);
    th->hstate = (struct hart_state*)transition.inject;

    commit_thread(th);
}

void 
exit_thread(void* val) {
    terminate_current_thread((ptr_t)val);
    schedule();
}

struct thread*
thread_find(struct proc_info* proc, tid_t tid)
{
    struct thread *pos, *n;
    llist_for_each(pos, n, &proc->threads, proc_sibs) {
        if (pos->tid == tid) {
            return pos;
        }
    }

    return NULL;
}

void
thread_stats_update(bool inbound, bool voluntary)
{
    struct thread_stats* stats;
    time_t now;
    
    now   = clock_systime();
    stats = &current_thread->stats;

    stats->at_user = !kernel_context(current_thread->hstate);

    if (!inbound) {
        if (kernel_process(current_thread->process) || 
            stats->at_user)
        {
            // exiting to user or kernel (kernel thread only), how graceful
            stats->last_leave = now;
        }
        else {
            // exiting to kernel, effectively reentry
            stats->last_reentry = now;
        }

        stats->last_resume = now;
        return;
    }

    stats->last_reentry = now;

    if (!stats->at_user)
    {
        // entering from kernel, it is a kernel preempt
        thread_stats_update_kpreempt();
        return;
    }

    // entering from user space, a clean entrance.

    if (!voluntary) {
        stats->entry_count_invol++;
    }
    else {
        stats->entry_count_vol++;
    }

    thread_stats_reset_kpreempt();
    stats->last_entry = now;
}

__DEFINE_LXSYSCALL3(int, th_create, tid_t*, tid, 
                        struct uthread_param*, thparam, void*, entry)
{
    no_preemption();

    struct thread* th = create_thread(__current, true);
    if (!th) {
        return EAGAIN;
    }

    ptr_t ustack_top;

    ustack_top = th->ustack_top;
    ustack_top = align_stack(ustack_top - sizeof(*thparam));

    memcpy((void*)ustack_top, thparam, sizeof(*thparam));

    th->ustack_top = ustack_top;
    start_thread(th, (ptr_t)entry);
    
    if (tid) {
        *tid = th->tid;
    }

    return 0;
}

__DEFINE_LXSYSCALL(tid_t, th_self)
{
    return current_thread->tid;
}

__DEFINE_LXSYSCALL1(void, th_exit, void*, val)
{
    exit_thread(val);
}

__DEFINE_LXSYSCALL2(int, th_join, tid_t, tid, void**, val_ptr)
{
    struct thread* th = thread_find(__current, tid);
    if (!th) {
        return EINVAL;
    }

    if (th == current_thread) {
        return EDEADLK;
    }

    while (!proc_terminated(th)) {
        yield_current();
    }

    if (val_ptr) {
        *val_ptr = (void*)th->exit_val;
    }

    no_preemption();
    destory_thread(th);

    return 0;
}

__DEFINE_LXSYSCALL1(int, th_detach, tid_t, tid)
{
    // can not detach the only thread
    if (__current->thread_count == 1) {
        return EINVAL;
    }

    struct thread* th = thread_find(__current, tid);
    if (!th) {
        return EINVAL;
    }

    detach_thread(th);
    return 0;
}

__DEFINE_LXSYSCALL2(int, th_kill, tid_t, tid, int, signum)
{
    struct thread* target = thread_find(__current, tid);
    if (!target) {
        return EINVAL;
    }

    if (signum > _SIG_NUM) {
        return EINVAL;
    }

    if (signum) {
        thread_setsignal(target, signum);
    }
    
    return 0;
}
