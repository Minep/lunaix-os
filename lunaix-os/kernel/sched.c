#include <arch/x86/interrupts.h>
#include <arch/x86/tss.h>

#include <hal/apic.h>
#include <hal/cpu.h>

#include <lunaix/mm/kalloc.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/signal.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syscall.h>
#include <lunaix/syslog.h>

#define MAX_PROCESS 512

volatile struct proc_info* __current;

struct proc_info dummy;

struct scheduler sched_ctx;

LOG_MODULE("SCHED")

void
sched_init()
{
    size_t pg_size = ROUNDUP(sizeof(struct proc_info) * MAX_PROCESS, 0x1000);

    for (size_t i = 0; i <= pg_size; i += 4096) {
        uintptr_t pa = pmm_alloc_page(KERNEL_PID, PP_FGPERSIST);
        vmm_set_mapping(
          PD_REFERENCED, PROC_START + i, pa, PG_PREM_RW, VMAP_NULL);
    }

    sched_ctx = (struct scheduler){ ._procs = (struct proc_info*)PROC_START,
                                    .ptable_len = 0,
                                    .procs_index = 0 };
}

void
run(struct proc_info* proc)
{
    proc->state = PROC_RUNNING;

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
                 "jmp switch_to\n" ::"r"(proc)); // kernel/asm/x86/interrupt.S
}

int
can_schedule(struct proc_info* proc)
{
    if (__SIGTEST(proc->sig_pending, _SIGKILL)) {
        // 如果进程受到SIGKILL，则直接终止，该进程不给予调度。
        terminate_proc(PEXITNUM(PEXITSIG, _SIGKILL));
        return 0;
    } else if (__SIGTEST(proc->sig_pending, _SIGCONT)) {
        __SIGCLEAR(proc->sig_pending, _SIGSTOP);
    } else if (__SIGTEST(proc->sig_pending, _SIGSTOP)) {
        // 如果进程受到SIGSTOP，则该进程不给予调度。
        return 0;
    }

    return 1;
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

    if (!(__current->state & ~PROC_RUNNING)) {
        __current->state = PROC_STOPPED;
    }

    // round-robin scheduler
redo:
    do {
        ptr = (ptr + 1) % sched_ctx.ptable_len;
        next = &sched_ctx._procs[ptr];
    } while (next->state != PROC_STOPPED && ptr != prev_ptr);

    sched_ctx.procs_index = ptr;

    if (!can_schedule(next)) {
        // 如果该进程不给予调度，则尝试重新选择
        goto redo;
    }

    run(next);
}

static void
proc_timer_callback(struct proc_info* proc)
{
    proc->timer = NULL;
    proc->state = PROC_STOPPED;
}

__DEFINE_LXSYSCALL1(unsigned int, sleep, unsigned int, seconds)
{
    // FIXME: sleep的实现或许需要改一下。专门绑一个计时器好像没有必要……
    if (!seconds) {
        return 0;
    }

    if (__current->timer) {
        return __current->timer->counter / timer_context()->running_frequency;
    }

    struct lx_timer* timer =
      timer_run_second(seconds, proc_timer_callback, __current, 0);
    __current->timer = timer;
    __current->intr_ctx.registers.eax = seconds;
    __current->state = PROC_BLOCKED;
    schedule();
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
    cpu_enable_interrupt();
repeat:
    llist_for_each(proc, n, &__current->children, siblings)
    {
        if (!~wpid || proc->pid == wpid || proc->pgid == -wpid) {
            if (proc->state == PROC_TERMNAT && !options) {
                status_flags |= PEXITTERM;
                goto done;
            }
            if (proc->state == PROC_STOPPED && (options & WUNTRACED)) {
                status_flags |= PEXITSTOP;
                goto done;
            }
        }
    }
    if ((options & WNOHANG)) {
        return 0;
    }
    // 放弃当前的运行机会
    sched_yield();
    goto repeat;

done:
    cpu_disable_interrupt();
    status_flags |= PEXITSIG * (proc->sig_inprogress != 0);
    *status = proc->exit_code | status_flags;
    return destroy_process(proc->pid);
}

struct proc_info*
alloc_process()
{
    pid_t i = 0;
    for (;
         i < sched_ctx.ptable_len && sched_ctx._procs[i].state != PROC_DESTROY;
         i++)
        ;

    if (i == MAX_PROCESS) {
        panick("Panic in Ponyville shimmer!");
    }

    if (i == sched_ctx.ptable_len) {
        sched_ctx.ptable_len++;
    }

    struct proc_info* proc = &sched_ctx._procs[i];
    memset(proc, 0, sizeof(*proc));

    proc->state = PROC_CREATED;
    proc->pid = i;
    proc->created = clock_systime();
    proc->pgid = proc->pid;

    llist_init_head(&proc->mm.regions);
    llist_init_head(&proc->children);
    llist_init_head(&proc->grp_member);

    return proc;
}

void
commit_process(struct proc_info* process)
{
    assert(process == &sched_ctx._procs[process->pid]);

    if (process->state != PROC_CREATED) {
        __current->k_status = LXINVL;
        return;
    }

    // every process is the child of first process (pid=1)
    if (!process->parent) {
        process->parent = &sched_ctx._procs[1];
    }

    llist_append(&process->parent->children, &process->siblings);

    process->state = PROC_STOPPED;
}

// from <kernel/process.c>
extern void
__del_pagetable(pid_t pid, uintptr_t mount_point);

pid_t
destroy_process(pid_t pid)
{
    int index = pid;
    if (index <= 0 || index > sched_ctx.ptable_len) {
        __current->k_status = LXINVLDPID;
        return;
    }
    struct proc_info* proc = &sched_ctx._procs[index];
    proc->state = PROC_DESTROY;
    llist_delete(&proc->siblings);

    struct mm_region *pos, *n;
    llist_for_each(pos, n, &proc->mm.regions.head, head)
    {
        lxfree(pos);
    }

    vmm_mount_pd(PD_MOUNT_1, proc->page_table);

    __del_pagetable(pid, PD_MOUNT_1);

    vmm_unmount_pd(PD_MOUNT_1);

    return pid;
}

void
terminate_proc(int exit_code)
{
    __current->state = PROC_TERMNAT;
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
    return &sched_ctx._procs[index];
}

int
orphaned_proc(pid_t pid)
{
    if (!pid)
        return 0;
    if (pid >= sched_ctx.ptable_len)
        return 0;
    struct proc_info* proc = &sched_ctx._procs[pid];
    struct proc_info* parent = proc->parent;

    // 如果其父进程的状态是terminated 或 destroy中的一种
    // 或者其父进程是在该进程之后创建的，那么该进程为孤儿进程
    return (parent->state & PROC_TERMMASK) || parent->created > proc->created;
}