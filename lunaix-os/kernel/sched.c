#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/mm/vmm.h>
#include <hal/cpu.h>
#include <arch/x86/interrupts.h>
#include <hal/apic.h>

#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syslog.h>

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

    __current = &dummy;
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
    } while((next->state != PROC_STOPPED && next->state != PROC_CREATED) && ptr != prev_ptr);
    
    sched_ctx.procs_index = ptr;
    
    __current->state = PROC_STOPPED;
    next->state = PROC_RUNNING;
    
    __current = next;

    cpu_lcr3(__current->page_table);

    apic_done_servicing();

    asm volatile ("pushl %0\n jmp soft_iret\n"::"r"(&__current->intr_ctx): "memory");
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
    
    process->parent = __current->pid;
    process->state = PROC_CREATED;

    sched_ctx._procs[index] = *process;
}

void destroy_process(pid_t pid) {
    int index = pid - 1;
    if (index < 0 || index > sched_ctx.ptable_len) {
        __current->k_status = LXINVLDPID;
        return;
    }

    sched_ctx._procs[index].state = PROC_DESTROY;

    // TODO: recycle the physical pages used by page tables
}

void terminate_process(int exit_code) {
    __current->state = PROC_TERMNAT;
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