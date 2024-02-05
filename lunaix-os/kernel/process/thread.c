#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/syslog.h>

#include <usr/lunaix/threads.h>

#include <sys/abi.h>
#include <sys/mm/mm_defs.h>

LOG_MODULE("THREAD")

static inline void 
inject_guardian_page(ptr_t vm_mnt, ptr_t va)
{
    vmm_set_mapping(vm_mnt, PG_ALIGN(va), 0, 0, VMAP_GUARDPAGE);
}

static ptr_t
__alloc_user_thread_stack(struct proc_info* proc, struct mm_region** stack_region, ptr_t vm_mnt)
{
    ptr_t th_stack_top = (proc->thread_count + 1) * USR_STACK_SIZE;
    th_stack_top = ROUNDUP(USR_STACK_END - th_stack_top, MEM_PAGE);

    struct mm_region* vmr;
    struct proc_mm* mm = vmspace(proc);
    struct mmap_param param = { .vms_mnt = vm_mnt,
                                .pvms = mm,
                                .mlen = USR_STACK_SIZE,
                                .proct = PROT_READ | PROT_WRITE,
                                .flags = MAP_ANON | MAP_PRIVATE,
                                .type = REGION_TYPE_STACK };

    int errno = mmap_user((void**)&th_stack_top, &vmr, th_stack_top, NULL, &param);

    if (errno) {
        WARN("failed to create user thread stack: %d", errno);
        return 0;
    }

    // Pre-allocate a page contains stack top, to avoid immediate trap to kernel
    //  upon thread execution
    ptr_t pa = pmm_alloc_page(0);
    ptr_t stack_top = align_stack(th_stack_top + USR_STACK_SIZE - 1);
    if (likely(pa)) {
        vmm_set_mapping(vm_mnt, PG_ALIGN(stack_top), 
                        pa, region_ptattr(vmr), 0);
    }

    inject_guardian_page(vm_mnt, vmr->start);

    *stack_region = vmr;

    return stack_top;
}

static ptr_t
__alloc_kernel_thread_stack(struct proc_info* proc, ptr_t vm_mnt)
{
    v_mapping mapping;
    ptr_t kstack = PG_ALIGN(KSTACK_AREA_END - KSTACK_SIZE);
    while (kstack >= KSTACK_AREA) {
        // first page in the kernel stack is guardian page
        if (!vmm_lookupat(vm_mnt, kstack + MEM_PAGE, &mapping) 
            || !PG_IS_PRESENT(mapping.flags)) 
        {
            break;
        }

        kstack -= KSTACK_SIZE;
    }

    if (kstack < KSTACK_AREA) {
        WARN("failed to create kernel stack: max stack num reach\n");
        return 0;
    }

    ptr_t pa = pmm_alloc_cpage(PN(KSTACK_SIZE) - 1, 0);

    if (!pa) {
        WARN("failed to create kernel stack: nomem\n");
        return 0;
    }

    inject_guardian_page(vm_mnt, kstack);
    for (size_t i = MEM_PAGE, j = 0; i < KSTACK_SIZE; i+=MEM_PAGE, j+=MEM_PAGE) {
        vmm_set_mapping(vm_mnt, kstack + i, pa + j, PG_PREM_RW, 0);
    }

    return align_stack(kstack + KSTACK_SIZE - 1);
}

void
thread_release_mem(struct thread* thread, ptr_t vm_mnt)
{
    for (size_t i = 0; i < KSTACK_SIZE; i+=MEM_PAGE) {
        ptr_t stack_page = PG_ALIGN(thread->kstack - i);
        vmm_del_mapping(vm_mnt, stack_page);
    }
    
    if (thread->ustack) {
        if ((thread->ustack->start & 0xfff)) {
            fail("invalid ustack struct");
        }
        mem_unmap_region(vm_mnt, thread->ustack);
    }
}

struct thread*
create_thread(struct proc_info* proc, ptr_t vm_mnt, bool with_ustack)
{
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
    th->kstack = kstack;
    th->ustack = ustack_region;

    return th;
}

void
start_thread(struct thread* th, ptr_t vm_mnt, ptr_t entry)
{
    assert(th && entry);
    
    struct transfer_context transfer;
    if (!kernel_addr(entry)) {
        assert(th->ustack);

        ptr_t ustack_top = align_stack(th->ustack->end - 1);
        ustack_top -= 16;   // pre_allocate a 16 byte for inject parameter
        thread_create_user_transfer(&transfer, th->kstack, ustack_top, entry);

        th->ustack_top = ustack_top;
    } 
    else {
        thread_create_kernel_transfer(&transfer, th->kstack, entry);
    }

    inject_transfer_context(vm_mnt, &transfer);
    th->intr_ctx = (isr_param*)transfer.inject;

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

__DEFINE_LXSYSCALL4(int, th_create, tid_t*, tid, struct uthread_info*, thinfo, 
                                    void*, entry, void*, param)
{
    struct thread* th = create_thread(__current, VMS_SELF, true);
    if (!th) {
        return EAGAIN;
    }

    start_thread(th, VMS_SELF, (ptr_t)entry);

    ptr_t ustack_top = th->ustack_top;
    *((void**)ustack_top) = param;

    thinfo->th_stack_sz = region_size(th->ustack);
    thinfo->th_stack_top = (void*)ustack_top;
    
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

    destory_thread(VMS_SELF, th);

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
