#include <sys/abi.h>
#include <sys/mm/mempart.h>

#include <sys/cpu.h>

#include <lunaix/fs/taskfs.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/mm/procvm.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/signal.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syscall.h>
#include <lunaix/syslog.h>
#include <lunaix/hart_state.h>
#include <lunaix/kpreempt.h>

#include <lunaix/generic/isrm.h>

#include <klibc/string.h>

struct thread empty_thread_obj;

volatile struct proc_info* __current;
volatile struct thread* current_thread = &empty_thread_obj;

struct scheduler sched_ctx;

struct cake_pile *proc_pile ,*thread_pile;

#define root_process   (sched_ctx.procs[1])

LOG_MODULE("SCHED")

void
sched_init()
{
    proc_pile = cake_new_pile("proc", sizeof(struct proc_info), 1, 0);
    thread_pile = cake_new_pile("thread", sizeof(struct thread), 1, 0);
    cake_set_constructor(proc_pile, cake_ctor_zeroing);
    cake_set_constructor(thread_pile, cake_ctor_zeroing);

    sched_ctx = (struct scheduler){
        .procs = vzalloc(PROC_TABLE_SIZE), .ptable_len = 0, .procs_index = 0};
    
    llist_init_head(&sched_ctx.sleepers);
}

void
run(struct thread* thread)
{
    thread->state = PS_RUNNING;
    thread->process->state = PS_RUNNING;
    thread->process->th_active = thread;

    procvm_mount_self(vmspace(thread->process));
    set_current_executing(thread);

    switch_context();

    fail("unexpected return from switching");
}

/*
    Currently, we do not allow self-destorying thread, doing
    so will eliminate current kernel stack which is disaster.
    A compromise solution is to perform a regular scan and 
    clean-up on these thread, in the preemptible kernel thread.
*/

void _preemptible
cleanup_detached_threads() {
    ensure_preempt_caller();

    // XXX may be a lock on sched_context will ben the most appropriate?
    cpu_disable_interrupt();

    int i = 0;
    struct thread *pos, *n;
    llist_for_each(pos, n, sched_ctx.threads, sched_sibs) {
        if (likely(!proc_terminated(pos) || !thread_detached(pos))) {
            continue;
        }

        struct proc_mm* mm = vmspace(pos->process);

        procvm_mount(mm);
        destory_thread(pos);
        procvm_unmount(mm);
        
        i++;
    }

    if (i) {
        INFO("cleaned %d terminated detached thread(s)", i);
    }

    cpu_enable_interrupt();
}

bool
can_schedule(struct thread* thread)
{
    if (!thread) {
        return 0;
    }

    if (proc_terminated(thread)) {
        return false;
    }

    if (preempt_check_stalled(thread)) {
        thread_flags_set(thread, TH_STALLED);
        return true;
    }

    if (unlikely(kernel_process(thread->process))) {
        // a kernel process is always runnable
        return thread->state == PS_READY;
    }

    struct sigctx* sh = &thread->sigctx;

    if ((thread->state & PS_PAUSED)) {
        return !!(sh->sig_pending & ~1);
    }

    if ((thread->state & PS_BLOCKED)) {
        return sigset_test(sh->sig_pending, _SIGINT);
    }

    if (sigset_test(sh->sig_pending, _SIGSTOP)) {
        // If one thread is experiencing SIGSTOP, then we know
        // all other threads are also SIGSTOP (as per POSIX-2008.1)
        // In which case, the entire process is stopped.
        thread->state = PS_STOPPED;
        return false;
    }
    
    if (sigset_test(sh->sig_pending, _SIGCONT)) {
        thread->state = PS_READY;
    }

    return (thread->state == PS_READY) \
            && proc_runnable(thread->process);
}

void
check_sleepers()
{
    struct thread *pos, *n;
    time_t now = clock_systime() / 1000;

    llist_for_each(pos, n, &sched_ctx.sleepers, sleep.sleepers)
    {
        if (proc_terminated(pos)) {
            goto del;
        }

        time_t wtime = pos->sleep.wakeup_time;
        time_t atime = pos->sleep.alarm_time;

        if (wtime && now >= wtime) {
            pos->sleep.wakeup_time = 0;
            pos->state = PS_READY;
        }

        if (atime && now >= atime) {
            pos->sleep.alarm_time = 0;
            thread_setsignal(pos, _SIGALRM);
        }

        if (!wtime && !atime) {
        del:
            llist_delete(&pos->sleep.sleepers);
        }
    }
}

void
schedule()
{
    assert(sched_ctx.ptable_len && sched_ctx.ttable_len);

    // 上下文切换相当的敏感！我们不希望任何的中断打乱栈的顺序……
    no_preemption();

    if (!(current_thread->state & ~PS_RUNNING)) {
        current_thread->state = PS_READY;
        __current->state = PS_READY;

    }

    procvm_unmount_self(vmspace(__current));
    check_sleepers();

    // round-robin scheduler
    
    struct thread* current = current_thread;
    struct thread* to_check = current;
    
    do {
        to_check = list_next(to_check, struct thread, sched_sibs);

        if (can_schedule(to_check)) {
            break;
        }

        if (to_check == current) {
            // FIXME do something less leathal here
            fail("Ran out of threads!")
            goto done;  
        }

    } while (1);

    sched_ctx.procs_index = to_check->process->pid;

done:
    isrm_notify_eos(0);
    run(to_check);

    fail("unexpected return from scheduler");
}

__DEFINE_LXSYSCALL1(unsigned int, sleep, unsigned int, seconds)
{
    if (!seconds) {
        return 0;
    }

    time_t systime = clock_systime() / 1000;
    struct haybed* bed = &current_thread->sleep;

    if (bed->wakeup_time) {
        return (bed->wakeup_time - systime);
    }

    bed->wakeup_time = systime + seconds;

    if (llist_empty(&bed->sleepers)) {
        llist_append(&sched_ctx.sleepers, &bed->sleepers);
    }

    store_retval(seconds);

    block_current_thread();
    schedule();

    return 0;
}

__DEFINE_LXSYSCALL1(unsigned int, alarm, unsigned int, seconds)
{
    struct haybed* bed = &current_thread->sleep;
    time_t prev_ddl = bed->alarm_time;
    time_t now = clock_systime() / 1000;

    bed->alarm_time = seconds ? now + seconds : 0;

    if (llist_empty(&bed->sleepers)) {
        llist_append(&sched_ctx.sleepers, &bed->sleepers);
    }

    return prev_ddl ? (prev_ddl - now) : 0;
}

__DEFINE_LXSYSCALL1(void, exit, int, status)
{
    terminate_current(status);
    schedule();
}

__DEFINE_LXSYSCALL(void, yield)
{
    schedule();
}

pid_t
_wait(pid_t wpid, int* status, int options);

__DEFINE_LXSYSCALL1(pid_t, wait, int*, status)
{
    return _wait(-1, status, 0);
}

__DEFINE_LXSYSCALL3(pid_t, waitpid, pid_t, pid, int*, status, int, options)
{
    return _wait(pid, status, options);
}

__DEFINE_LXSYSCALL(int, geterrno)
{
    return current_thread->syscall_ret;
}

pid_t
_wait(pid_t wpid, int* status, int options)
{
    pid_t cur = __current->pid;
    int status_flags = 0;
    struct proc_info *proc, *n;
    if (llist_empty(&__current->children)) {
        return -1;
    }

    wpid = wpid ? wpid : -__current->pgid;

repeat:
    llist_for_each(proc, n, &__current->children, siblings)
    {
        if (!~wpid || proc->pid == wpid || proc->pgid == -wpid) {
            if (proc->state == PS_TERMNAT && !options) {
                status_flags |= PEXITTERM;
                goto done;
            }
            if (proc->state == PS_READY && (options & WUNTRACED)) {
                status_flags |= PEXITSTOP;
                goto done;
            }
        }
    }
    if ((options & WNOHANG)) {
        return 0;
    }
    // 放弃当前的运行机会
    yield_current();
    goto repeat;

done:
    if (status) {
        *status = PEXITNUM(status_flags, proc->exit_code);
    }
    return destroy_process(proc->pid);
}

static inline pid_t
get_free_pid() {
    pid_t i = 0;
    
    for (; i < sched_ctx.ptable_len && sched_ctx.procs[i]; i++)
        ;
    
    if (unlikely(i == MAX_PROCESS)) {
        panick("Panic in Ponyville shimmer!");
    }

    return i;
}

struct thread*
alloc_thread(struct proc_info* process) {
    if (process->thread_count >= MAX_THREAD_PP) {
        return NULL;
    }
    
    struct thread* th = cake_grab(thread_pile);

    th->process = process;
    th->created = clock_systime();

    // FIXME we need a better tid allocation method!
    th->tid = th->created;
    th->tid = (th->created ^ ((ptr_t)th)) % MAX_THREAD_PP;

    th->state = PS_CREATED;
    
    llist_init_head(&th->sleep.sleepers);
    llist_init_head(&th->sched_sibs);
    llist_init_head(&th->proc_sibs);
    waitq_init(&th->waitqueue);

    return th;
}

struct proc_info*
alloc_process()
{
    pid_t i = get_free_pid();

    if (i == sched_ctx.ptable_len) {
        sched_ctx.ptable_len++;
    }

    struct proc_info* proc = cake_grab(proc_pile);
    if (!proc) {
        return NULL;
    }

    proc->state = PS_CREATED;
    proc->pid = i;
    proc->created = clock_systime();
    proc->pgid = proc->pid;

    proc->sigreg = vzalloc(sizeof(struct sigregistry));
    proc->fdtable = vzalloc(sizeof(struct v_fdtable));

    proc->mm = procvm_create(proc);
    
    llist_init_head(&proc->tasks);
    llist_init_head(&proc->children);
    llist_init_head(&proc->grp_member);
    llist_init_head(&proc->threads);

    iopoll_init(&proc->pollctx);

    sched_ctx.procs[i] = proc;

    return proc;
}

void
commit_thread(struct thread* thread) {
    struct proc_info* process = thread->process;

    assert(process && !proc_terminated(process));

    llist_append(&process->threads, &thread->proc_sibs);
    
    if (sched_ctx.threads) {
        llist_append(sched_ctx.threads, &thread->sched_sibs);
    } else {
        sched_ctx.threads = &thread->sched_sibs;
    }

    sched_ctx.ttable_len++;
    process->thread_count++;
    thread->state = PS_READY;
}

void
commit_process(struct proc_info* process)
{
    assert(process == sched_ctx.procs[process->pid]);
    assert(process->state == PS_CREATED);

    // every process is the child of first process (pid=1)
    if (!process->parent) {
        if (likely(!kernel_process(process))) {
            process->parent = root_process;
        } else {
            process->parent = process;
        }
    } else {
        assert(!proc_terminated(process->parent));
    }

    if (sched_ctx.proc_list) {
        llist_append(sched_ctx.proc_list, &process->tasks);
    } else {
        sched_ctx.proc_list = &process->tasks;
    }

    llist_append(&process->parent->children, &process->siblings);

    process->state = PS_READY;
}

void
destory_thread(struct thread* thread) 
{
    cake_ensure_valid(thread);
    
    struct proc_info* proc = thread->process;

    llist_delete(&thread->sched_sibs);
    llist_delete(&thread->proc_sibs);
    llist_delete(&thread->sleep.sleepers);
    waitq_cancel_wait(&thread->waitqueue);

    thread_release_mem(thread);

    proc->thread_count--;
    sched_ctx.ttable_len--;

    cake_release(thread_pile, thread);
}

static void
orphan_children(struct proc_info* proc)
{
    struct proc_info *root;
    struct proc_info *pos, *n;

    root = root_process;

    llist_for_each(pos, n, &proc->children, siblings) {
        pos->parent = root;
        llist_append(&root->children, &pos->siblings);
    }
}

void 
delete_process(struct proc_info* proc)
{
    pid_t pid = proc->pid;
    struct proc_mm* mm = vmspace(proc);

    assert(pid);    // long live the pid0 !!

    sched_ctx.procs[pid] = NULL;

    llist_delete(&proc->siblings);
    llist_delete(&proc->grp_member);
    llist_delete(&proc->tasks);

    iopoll_free(proc);

    taskfs_invalidate(pid);

    if (proc->cwd) {
        vfs_unref_dnode(proc->cwd);
    }

    if (proc->cmd) {
        vfree(proc->cmd);
    }

    for (size_t i = 0; i < VFS_MAX_FD; i++) {
        struct v_fd* fd = proc->fdtable->fds[i];
        if (fd) {
            vfs_pclose(fd->file, pid);
            vfs_free_fd(fd);
        }
    }

    vfree(proc->fdtable);

    signal_free_registry(proc->sigreg);

    procvm_mount(mm);
    
    struct thread *pos, *n;
    llist_for_each(pos, n, &proc->threads, proc_sibs) {
        // terminate and destory all thread unconditionally
        destory_thread(pos);
    }

    orphan_children(proc);

    procvm_unmount_release(mm);

    cake_release(proc_pile, proc);
}

pid_t
destroy_process(pid_t pid)
{    
    int index = pid;
    if (index <= 0 || index > sched_ctx.ptable_len) {
        syscall_result(EINVAL);
        return -1;
    }

    struct proc_info* proc = sched_ctx.procs[index];
    delete_process(proc);

    return pid;
}

static void 
terminate_proc_only(struct proc_info* proc, int exit_code) {
    assert(proc->pid != 0);

    proc->state = PS_TERMNAT;
    proc->exit_code = exit_code;

    proc_setsignal(proc->parent, _SIGCHLD);
}

void
terminate_thread(struct thread* thread, ptr_t val) {
    thread->exit_val = val;
    thread->state = PS_TERMNAT;

    struct proc_info* proc = thread->process;
    if (proc->thread_count == 1) {
        terminate_proc_only(thread->process, 0);
    }
}

void
terminate_current_thread(ptr_t val) {
    terminate_thread(current_thread, val);
}

void 
terminate_proccess(struct proc_info* proc, int exit_code) {
    assert(!kernel_process(proc));

    if (proc->pid == 1) {
        panick("Attempt to kill init");
    }

    terminate_proc_only(proc, exit_code);

    struct thread *pos, *n;
    llist_for_each(pos, n, &proc->threads, proc_sibs) {
        pos->state = PS_TERMNAT;
    }
}

void
terminate_current(int exit_code)
{
    terminate_proccess(__current, exit_code);
}

struct proc_info*
get_process(pid_t pid)
{
    int index = pid;
    if (index < 0 || index > sched_ctx.ptable_len) {
        return NULL;
    }
    return sched_ctx.procs[index];
}

int
orphaned_proc(pid_t pid)
{
    if (!pid)
        return 0;
    if (pid >= sched_ctx.ptable_len)
        return 0;
    struct proc_info* proc = sched_ctx.procs[pid];
    struct proc_info* parent = proc->parent;

    // 如果其父进程的状态是terminated 或 destroy中的一种
    // 或者其父进程是在该进程之后创建的，那么该进程为孤儿进程
    return proc_terminated(parent) || parent->created > proc->created;
}