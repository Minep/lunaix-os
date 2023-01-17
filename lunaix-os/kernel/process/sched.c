#include <arch/x86/interrupts.h>
#include <arch/x86/tss.h>

#include <hal/apic.h>
#include <hal/cpu.h>

#include <lunaix/fs/taskfs.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/kalloc.h>
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

    sched_ctx = (struct scheduler){ ._procs = vzalloc(PROC_TABLE_SIZE),
                                    .ptable_len = 0,
                                    .procs_index = 0 };

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

    // memset to 0
    dummy_proc = (struct proc_info){};
    dummy_proc.intr_ctx = (isr_param){
        .registers = { .ds = KDATA_SEG,
                       .es = KDATA_SEG,
                       .fs = KDATA_SEG,
                       .gs = KDATA_SEG,
                       .esp = (void*)dummy_stack + DUMMY_STACK_SIZE - 20 },
        .cs = KCODE_SEG,
        .eip = (void*)my_dummy,
        .ss = KDATA_SEG,
        .eflags = cpu_reflags() | 0x0200
    };

    *(u32_t*)(&dummy_stack[DUMMY_STACK_SIZE - 4]) = dummy_proc.intr_ctx.eflags;
    *(u32_t*)(&dummy_stack[DUMMY_STACK_SIZE - 8]) = KCODE_SEG;
    *(u32_t*)(&dummy_stack[DUMMY_STACK_SIZE - 12]) = dummy_proc.intr_ctx.eip;

    dummy_proc.page_table = cpu_rcr3();
    dummy_proc.state = PS_READY;
    dummy_proc.parent = &dummy_proc;
    dummy_proc.pid = KERNEL_PID;

    __current = &dummy_proc;
}

void
run(struct proc_info* proc)
{
    proc->state = PS_RUNNING;

    /*
        将tss.esp0设置为上次调度前的esp值。
        当处理信号时，上下文信息是不会恢复的，而是保存在用户栈中，然后直接跳转进位于用户空间的sig_wrapper进行
          信号的处理。当用户自定义的信号处理函数返回时，sigreturn的系统调用才开始进行上下文的恢复（或者说是进行
          另一次调度。
        由于这中间没有进行地址空间的交换，所以第二次跳转使用的是同一个内核栈，而之前默认tss.esp0的值是永远指向最顶部
        这样一来就有可能会覆盖更早的上下文信息（比如嵌套的信号捕获函数）
    */
    tss_update_esp(proc->intr_ctx.registers.esp);

    apic_done_servicing();

    asm volatile("pushl %0\n"
                 "jmp switch_to\n" ::"r"(proc)
                 : "memory"); // kernel/asm/x86/interrupt.S
}

int
can_schedule(struct proc_info* proc)
{
    if (__SIGTEST(proc->sig_pending, _SIGCONT)) {
        __SIGCLEAR(proc->sig_pending, _SIGSTOP);
    } else if (__SIGTEST(proc->sig_pending, _SIGSTOP)) {
        // 如果进程受到SIGSTOP，则该进程不给予调度。
        return 0;
    }

    return 1;
}

void
check_sleepers()
{
    struct proc_info* leader = sched_ctx._procs[0];
    struct proc_info *pos, *n;
    time_t now = clock_systime();
    llist_for_each(pos, n, &leader->sleep.sleepers, sleep.sleepers)
    {
        if (PROC_TERMINATED(pos->state)) {
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
            __SIGSET(pos->sig_pending, _SIGALRM);
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

    if (!(__current->state & ~PS_RUNNING)) {
        __current->state = PS_READY;
    }

    check_sleepers();

    // round-robin scheduler
redo:
    do {
        ptr = (ptr + 1) % sched_ctx.ptable_len;
        next = sched_ctx._procs[ptr];
    } while (!next || (next->state != PS_READY && ptr != prev_ptr));

    sched_ctx.procs_index = ptr;

    if (next->state != PS_READY) {
        // schedule the dummy process if we're out of choice
        next = &dummy_proc;
        goto done;
    }

    if (!can_schedule(next)) {
        // 如果该进程不给予调度，则尝试重新选择
        goto redo;
    }

done:
    run(next);
}

void
sched_yieldk()
{
    cpu_enable_interrupt();
    cpu_int(LUNAIX_SCHED);
}

__DEFINE_LXSYSCALL1(unsigned int, sleep, unsigned int, seconds)
{
    if (!seconds) {
        return 0;
    }

    if (__current->sleep.wakeup_time) {
        return (__current->sleep.wakeup_time - clock_systime()) / 1000U;
    }

    struct proc_info* root_proc = sched_ctx._procs[0];
    __current->sleep.wakeup_time = clock_systime() + seconds * 1000;

    if (llist_empty(&__current->sleep.sleepers)) {
        llist_append(&root_proc->sleep.sleepers, &__current->sleep.sleepers);
    }

    __current->intr_ctx.registers.eax = seconds;

    block_current();
    schedule();
}

__DEFINE_LXSYSCALL1(unsigned int, alarm, unsigned int, seconds)
{
    time_t prev_ddl = __current->sleep.alarm_time;
    time_t now = clock_systime();

    __current->sleep.alarm_time = seconds ? now + seconds * 1000 : 0;

    struct proc_info* root_proc = sched_ctx._procs[0];
    if (llist_empty(&__current->sleep.sleepers)) {
        llist_append(&root_proc->sleep.sleepers, &__current->sleep.sleepers);
    }

    return prev_ddl ? (prev_ddl - now) / 1000 : 0;
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
    status_flags |= PEXITSIG * (proc->sig_inprogress != 0);
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
    proc->created = clock_systime();
    proc->pgid = proc->pid;
    proc->fdtable = vzalloc(sizeof(struct v_fdtable));
    proc->fxstate =
      vzalloc_dma(512); // FXSAVE需要十六位对齐地址，使用DMA块（128位对齐）

    llist_init_head(&proc->mm.regions.head);
    llist_init_head(&proc->tasks);
    llist_init_head(&proc->children);
    llist_init_head(&proc->grp_member);
    llist_init_head(&proc->sleep.sleepers);
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
__del_pagetable(pid_t pid, uintptr_t mount_point);

pid_t
destroy_process(pid_t pid)
{
    int index = pid;
    if (index <= 0 || index > sched_ctx.ptable_len) {
        __current->k_status = EINVAL;
        return;
    }
    struct proc_info* proc = sched_ctx._procs[index];
    sched_ctx._procs[index] = 0;

    llist_delete(&proc->siblings);
    llist_delete(&proc->grp_member);
    llist_delete(&proc->tasks);
    llist_delete(&proc->sleep.sleepers);

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
    vfree_dma(proc->fxstate);

    struct mm_region *pos, *n;
    llist_for_each(pos, n, &proc->mm.regions.head, head)
    {
        vfree(pos);
    }

    vmm_mount_pd(PD_MOUNT_1, proc->page_table);

    __del_pagetable(pid, PD_MOUNT_1);

    vmm_unmount_pd(PD_MOUNT_1);

    cake_release(proc_pile, proc);

    return pid;
}

void
terminate_proc(int exit_code)
{
    __current->state = PS_TERMNAT;
    __current->exit_code = exit_code;

    __SIGSET(__current->parent->sig_pending, _SIGCHLD);
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
    return PROC_TERMINATED(parent->state) || parent->created > proc->created;
}