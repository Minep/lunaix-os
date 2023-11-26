#include <sys/abi.h>
#include <sys/interrupts.h>
#include <sys/mm/mempart.h>

#include <hal/intc.h>
#include <sys/cpu.h>

#include <lunaix/fs/taskfs.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/signal.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syscall.h>
#include <lunaix/syslog.h>

#include <klibc/string.h>

volatile struct proc_info* __current;

static struct proc_info dummy_proc;

struct proc_info dummy;

struct scheduler sched_ctx;

struct cake_pile* proc_pile;

LOG_MODULE("SCHED")

void
sched_init_dummy();

void
sched_init()
{
    proc_pile = cake_new_pile("proc", sizeof(struct proc_info), 1, 0);
    cake_set_constructor(proc_pile, cake_ctor_zeroing);

    sched_ctx = (struct scheduler){
        ._procs = vzalloc(PROC_TABLE_SIZE), .ptable_len = 0, .procs_index = 0};

    // TODO initialize dummy_proc
    sched_init_dummy();
}

#define DUMMY_STACK_SIZE 2048

void
sched_init_dummy()
{
    // This surely need to be simplified or encapsulated!
    // It is a living nightmare!

    extern void my_dummy();
    static char dummy_stack[DUMMY_STACK_SIZE] __attribute__((aligned(16)));

    ptr_t stktop = (ptr_t)dummy_stack + DUMMY_STACK_SIZE;

    dummy_proc = (struct proc_info){};

    proc_init_transfer(&dummy_proc, stktop, (ptr_t)my_dummy, TRANSFER_IE);

    dummy_proc.page_table = cpu_ldvmspace();
    dummy_proc.state = PS_READY;
    dummy_proc.parent = &dummy_proc;
    dummy_proc.pid = KERNEL_PID;

    __current = &dummy_proc;
}

void
run(struct proc_info* proc)
{
    proc->state = PS_RUNNING;

    intc_notify_eos(0);
    switch_context(proc);
}

int
can_schedule(struct proc_info* proc)
{
    if (!proc) {
        return 0;
    }

    struct sighail* sh = &proc->sigctx;

    if ((proc->state & PS_PAUSED)) {
        return !!(sh->sig_pending & ~1);
    }
    if ((proc->state & PS_BLOCKED)) {
        return sigset_test(sh->sig_pending, _SIGINT);
    }

    if (sigset_test(sh->sig_pending, _SIGCONT)) {
        sigset_clear(sh->sig_pending, _SIGSTOP);
    } else if (sigset_test(sh->sig_pending, _SIGSTOP)) {
        // 如果进程受到SIGSTOP，则该进程不给予调度。
        return 0;
    }

    return (proc->state == PS_READY);
}

void
check_sleepers()
{
    struct proc_info* leader = sched_ctx._procs[0];
    struct proc_info *pos, *n;
    time_t now = clock_systime() / 1000;
    llist_for_each(pos, n, &leader->sleep.sleepers, sleep.sleepers)
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
            proc_setsignal(pos, _SIGALRM);
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
    if (!sched_ctx.ptable_len) {
        return;
    }

    // 上下文切换相当的敏感！我们不希望任何的中断打乱栈的顺序……
    cpu_disable_interrupt();
    struct proc_info* next;
    int prev_ptr = sched_ctx.procs_index;
    int ptr = prev_ptr;
    int found = 0;

    if (!(__current->state & ~PS_RUNNING)) {
        __current->state = PS_READY;
    }

    check_sleepers();

    // round-robin scheduler
    do {
        ptr = (ptr + 1) % sched_ctx.ptable_len;
        next = sched_ctx._procs[ptr];

        if (!(found = can_schedule(next))) {
            if (ptr == prev_ptr) {
                next = &dummy_proc;
                goto done;
            }
        }
    } while (!found);

    sched_ctx.procs_index = ptr;

done:
    run(next);
}

void
sched_yieldk()
{
    cpu_enable_interrupt();
    cpu_trap_sched();
}

__DEFINE_LXSYSCALL1(unsigned int, sleep, unsigned int, seconds)
{
    if (!seconds) {
        return 0;
    }

    time_t systime = clock_systime() / 1000;

    if (__current->sleep.wakeup_time) {
        return (__current->sleep.wakeup_time - systime);
    }

    struct proc_info* root_proc = sched_ctx._procs[0];
    __current->sleep.wakeup_time = systime + seconds;

    if (llist_empty(&__current->sleep.sleepers)) {
        llist_append(&root_proc->sleep.sleepers, &__current->sleep.sleepers);
    }

    store_retval(seconds);

    block_current();
    schedule();

    return 0;
}

__DEFINE_LXSYSCALL1(unsigned int, alarm, unsigned int, seconds)
{
    time_t prev_ddl = __current->sleep.alarm_time;
    time_t now = clock_systime() / 1000;

    __current->sleep.alarm_time = seconds ? now + seconds : 0;

    struct proc_info* root_proc = sched_ctx._procs[0];
    if (llist_empty(&__current->sleep.sleepers)) {
        llist_append(&root_proc->sleep.sleepers, &__current->sleep.sleepers);
    }

    return prev_ddl ? (prev_ddl - now) : 0;
}

__DEFINE_LXSYSCALL1(void, exit, int, status)
{
    terminate_proc(status);
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
    return __current->k_status;
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
    sched_yieldk();
    goto repeat;

done:
    if (status) {
        *status = proc->exit_code | status_flags;
    }
    return destroy_process(proc->pid);
}

struct proc_info*
alloc_process()
{
    pid_t i = 0;
    for (; i < sched_ctx.ptable_len && sched_ctx._procs[i]; i++)
        ;

    if (i == MAX_PROCESS) {
        panick("Panic in Ponyville shimmer!");
    }

    if (i == sched_ctx.ptable_len) {
        sched_ctx.ptable_len++;
    }

    struct proc_info* proc = cake_grab(proc_pile);

    proc->state = PS_CREATED;
    proc->pid = i;
    proc->mm.pid = i;
    proc->created = clock_systime();
    proc->pgid = proc->pid;
    proc->fdtable = vzalloc(sizeof(struct v_fdtable));

    llist_init_head(&proc->mm.regions);
    llist_init_head(&proc->tasks);
    llist_init_head(&proc->children);
    llist_init_head(&proc->grp_member);
    llist_init_head(&proc->sleep.sleepers);

    iopoll_init(&proc->pollctx);
    waitq_init(&proc->waitqueue);

    sched_ctx._procs[i] = proc;

    return proc;
}

void
commit_process(struct proc_info* process)
{
    assert(process == sched_ctx._procs[process->pid]);

    if (process->state != PS_CREATED) {
        __current->k_status = EINVAL;
        return;
    }

    // every process is the child of first process (pid=1)
    if (!process->parent) {
        process->parent = sched_ctx._procs[1];
    }

    llist_append(&process->parent->children, &process->siblings);
    llist_append(&sched_ctx._procs[0]->tasks, &process->tasks);

    process->state = PS_READY;
}

// from <kernel/process.c>
extern void
__del_pagetable(pid_t pid, ptr_t mount_point);

pid_t
destroy_process(pid_t pid)
{
    int index = pid;
    if (index <= 0 || index > sched_ctx.ptable_len) {
        __current->k_status = EINVAL;
        return -1;
    }

    struct proc_info* proc = sched_ctx._procs[index];
    sched_ctx._procs[index] = 0;

    llist_delete(&proc->siblings);
    llist_delete(&proc->grp_member);
    llist_delete(&proc->tasks);
    llist_delete(&proc->sleep.sleepers);

    iopoll_free(pid, &proc->pollctx);

    taskfs_invalidate(pid);

    if (proc->cwd) {
        vfs_unref_dnode(proc->cwd);
    }

    for (size_t i = 0; i < VFS_MAX_FD; i++) {
        struct v_fd* fd = proc->fdtable->fds[i];
        if (fd) {
            vfs_pclose(fd->file, pid);
            vfs_free_fd(fd);
        }
    }

    vfree(proc->fdtable);

    vmm_mount_pd(VMS_MOUNT_1, proc->page_table);

    struct mm_region *pos, *n;
    llist_for_each(pos, n, &proc->mm.regions, head)
    {
        mem_sync_pages(VMS_MOUNT_1, pos, pos->start, pos->end - pos->start, 0);
        region_release(pos);
    }

    __del_pagetable(pid, VMS_MOUNT_1);

    vmm_unmount_pd(VMS_MOUNT_1);

    cake_release(proc_pile, proc);

    return pid;
}

void
terminate_proc(int exit_code)
{
    __current->state = PS_TERMNAT;
    __current->exit_code = exit_code;

    proc_setsignal(__current->parent, _SIGCHLD);
}

struct proc_info*
get_process(pid_t pid)
{
    int index = pid;
    if (index < 0 || index > sched_ctx.ptable_len) {
        return NULL;
    }
    return sched_ctx._procs[index];
}

int
orphaned_proc(pid_t pid)
{
    if (!pid)
        return 0;
    if (pid >= sched_ctx.ptable_len)
        return 0;
    struct proc_info* proc = sched_ctx._procs[pid];
    struct proc_info* parent = proc->parent;

    // 如果其父进程的状态是terminated 或 destroy中的一种
    // 或者其父进程是在该进程之后创建的，那么该进程为孤儿进程
    return proc_terminated(parent) || parent->created > proc->created;
}