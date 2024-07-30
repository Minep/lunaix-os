#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/page.h>
#include <lunaix/syslog.h>

#include <usr/lunaix/threads.h>

#include <sys/abi.h>
#include <sys/mm/mm_defs.h>

LOG_MODULE("THREAD")

static ptr_t
__alloc_user_thread_stack(struct proc_info* proc, 
                          struct mm_region** stack_region, ptr_t vm_mnt)
{
    ptr_t th_stack_top = (proc->thread_count + 1) * USR_STACK_SIZE_THREAD;
    th_stack_top = ROUNDUP(USR_STACK_END - th_stack_top, PAGE_SIZE);

    struct mm_region* vmr;
    struct proc_mm* mm = vmspace(proc);
    struct mmap_param param = { .vms_mnt = vm_mnt,
                                .pvms = mm,
                                .mlen = USR_STACK_SIZE_THREAD,
                                .proct = PROT_READ | PROT_WRITE,
                                .flags = MAP_ANON | MAP_PRIVATE,
                                .type = REGION_TYPE_STACK };

    int errno;
    
    errno = mmap_user((void**)&th_stack_top, &vmr, th_stack_top, NULL, &param);
    if (errno) {
        WARN("failed to create user thread stack: %d", errno);
        return 0;
    }

    pte_t* guardp = mkptep_va(vm_mnt, vmr->start);
    set_pte(guardp, guard_pte);

    *stack_region = vmr;

    ptr_t stack_top = align_stack(th_stack_top + USR_STACK_SIZE_THREAD - 1);
    return stack_top;
}

static ptr_t
__alloc_kernel_thread_stack(struct proc_info* proc, ptr_t vm_mnt)
{
    pfn_t kstack_top = leaf_count(KSTACK_AREA_END);
    pfn_t kstack_end = pfn(KSTACK_AREA);
    pte_t* ptep      = mkptep_pn(vm_mnt, kstack_top);
    while (ptep_pfn(ptep) > kstack_end) {
        ptep -= KSTACK_PAGES + 1;

        pte_t pte = pte_at(ptep + 1);
        if (pte_isnull(pte)) {
            goto found;
        }
    }

    WARN("failed to create kernel stack: max stack num reach\n");
    return 0;

found:;
    unsigned int po = count_order(KSTACK_PAGES);
    struct leaflet* leaflet = alloc_leaflet(po);

    if (!leaflet) {
        WARN("failed to create kernel stack: nomem\n");
        return 0;
    }

    set_pte(ptep, guard_pte);
    ptep_map_leaflet(ptep + 1, mkpte_prot(KERNEL_DATA), leaflet);

    ptep += KSTACK_PAGES;
    return align_stack(ptep_va(ptep, LFT_SIZE) - 1);
}

void
thread_release_mem(struct thread* thread)
{
    struct leaflet* leaflet;
    struct proc_mm* mm = vmspace(thread->process);
    ptr_t vm_mnt = mm->vm_mnt;

    // Ensure we have mounted
    assert(vm_mnt);

    pte_t* ptep = mkptep_va(vm_mnt, thread->kstack);
    leaflet = pte_leaflet(*ptep);
    
    ptep -= KSTACK_PAGES - 1;
    set_pte(ptep, null_pte);
    ptep_unmap_leaflet(ptep + 1, leaflet);

    leaflet_return(leaflet);
    
    if (thread->ustack) {
        if ((thread->ustack->start & 0xfff)) {
            fail("invalid ustack struct");
        }
        mem_unmap_region(vm_mnt, thread->ustack);
    }
}

struct thread*
create_thread(struct proc_info* proc, bool with_ustack)
{
    struct proc_mm* mm = vmspace(proc);
    assert(mm->vm_mnt);

    ptr_t vm_mnt = mm->vm_mnt;
    struct mm_region* ustack_region = NULL;
    if (with_ustack && 
        !(__alloc_user_thread_stack(proc, &ustack_region, vm_mnt))) 
    {
        return NULL;
    }

    ptr_t kstack = __alloc_kernel_thread_stack(proc, vm_mnt);
    if (!kstack) {
        mem_unmap_region(vm_mnt, ustack_region);
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
    assert(th && entry);
    struct proc_mm* mm = vmspace(th->process);

    assert(mm->vm_mnt);
    
    struct hart_transition transition;
    if (!kernel_addr(entry)) {
        assert(th->ustack);

        hart_user_transfer(&transition, th->kstack, th->ustack_top, entry);
    } 
    else {
        hart_kernel_transfer(&transition, th->kstack, entry);
    }

    install_hart_transition(mm->vm_mnt, &transition);
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

__DEFINE_LXSYSCALL3(int, th_create, tid_t*, tid, 
                        struct uthread_param*, thparam, void*, entry)
{
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
        sched_pass();
    }

    if (val_ptr) {
        *val_ptr = (void*)th->exit_val;
    }

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
