#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/mm/vmm.h>
#include <hal/cpu.h>
#include <arch/x86/interrupts.h>
#include <hal/apic.h>

#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syslog.h>
#include <lunaix/syscall.h>

#define MAX_PROCESS 512

struct proc_info* __current;
struct proc_info dummy;

extern void __proc_table;

struct scheduler sched_ctx;

LOG_MODULE("SCHED")

void sched_init() {
    size_t pg_size = ROUNDUP(sizeof(struct proc_info) * MAX_PROCESS, 0x1000);
    assert_msg(
        vmm_alloc_pages(KERNEL_PID, &__proc_table, pg_size, PG_PREM_RW, PP_FGPERSIST), 
        "Fail to allocate proc table"
    );
    
    sched_ctx = (struct scheduler) {
        ._procs = (struct proc_info*) &__proc_table,
        .ptable_len = 0,
        .procs_index = 0
    };
}

void run(struct proc_info* proc) {
    if (!(__current->state & ~PROC_RUNNING)) {
        __current->state = PROC_STOPPED;
    }
    proc->state = PROC_RUNNING;
    
    __current = proc;

    cpu_lcr3(__current->page_table);

    apic_done_servicing();

    asm volatile (
        "pushl %0\n"
        "jmp soft_iret\n"::"r"(&__current->intr_ctx): "memory");
}

void schedule() {
    if (!sched_ctx.ptable_len) {
        return;
    }

    struct proc_info* next;
    int prev_ptr = sched_ctx.procs_index;
    int ptr = prev_ptr;
    // round-robin scheduler
    do {
        ptr = (ptr + 1) % sched_ctx.ptable_len;
        next = &sched_ctx._procs[ptr];
    } while(next->state != PROC_STOPPED && ptr != prev_ptr);
    
    sched_ctx.procs_index = ptr;
    
    run(next);
}

static void proc_timer_callback(struct proc_info* proc) {
    proc->timer = NULL;
    proc->state = PROC_STOPPED;
}

__DEFINE_LXSYSCALL1(unsigned int, sleep, unsigned int, seconds) {
    if (!seconds) {
        return 0;
    }
    if (__current->timer) {
        return __current->timer->counter / timer_context()->running_frequency;
    }

    struct lx_timer* timer = timer_run_second(seconds, proc_timer_callback, __current, 0);
    __current->timer = timer;
    __current->intr_ctx.registers.eax = seconds;
    __current->state = PROC_BLOCKED;
    schedule();
}

__DEFINE_LXSYSCALL1(void, exit, int, status) {
    terminate_proc(status);
}

__DEFINE_LXSYSCALL(void, yield) {
    schedule();
}

pid_t alloc_pid() {
    pid_t i = 0;
    for (; i < sched_ctx.ptable_len && sched_ctx._procs[i].state != PROC_DESTROY; i++);

    if (i == MAX_PROCESS) {
        __current->k_status = LXPROCFULL;
        return -1;
    }
    return i + 1;
}

void push_process(struct proc_info* process) {
    int index = process->pid - 1;
    if (index < 0 || index > sched_ctx.ptable_len) {
        __current->k_status = LXINVLDPID;
        return;
    }
    
    if (index == sched_ctx.ptable_len) {
        sched_ctx.ptable_len++;
    }
    
    // every process is the parent of first process (pid=1)
    process->parent = process->parent ? process->parent : &sched_ctx._procs;
    process->state = PROC_STOPPED;

    sched_ctx._procs[index] = *process;
}

void destroy_process(pid_t pid) {
    int index = pid - 1;
    if (index <= 0 || index > sched_ctx.ptable_len) {
        __current->k_status = LXINVLDPID;
        return;
    }

    sched_ctx._procs[index].state = PROC_DESTROY;

    // TODO: recycle the physical pages used by page tables
}

void terminate_proc(int exit_code) {
    __current->state = exit_code < 0 ? PROC_SPOILED : PROC_TERMNAT;
    __current->exit_code = exit_code;

    schedule();
}

struct proc_info* get_process(pid_t pid) {
    int index = pid - 1;
    if (index < 0 || index > sched_ctx.ptable_len) {
        return NULL;
    }
    return &sched_ctx._procs[index];
}

int orphaned_proc(pid_t pid) {
    if(!pid) return 0;
    if(pid >= sched_ctx.ptable_len) return 0;
    struct proc_info* proc = &sched_ctx._procs[pid-1];
    struct proc_info* parent = proc->parent;
    
    // 如果其父进程的状态是terminated, spoiled 或 destroy中的一种
    // 或者其父进程是在该进程之后创建的，那么该进程为孤儿进程
    return (parent->state & 0xe) || parent->created > proc->created;
}