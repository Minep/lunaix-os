#ifndef __LUNAIX_PROCESS_H
#define __LUNAIX_PROCESS_H

#include <arch/x86/interrupts.h>
#include <lunaix/clock.h>
#include <lunaix/mm/mm.h>
#include <lunaix/signal.h>
#include <lunaix/timer.h>
#include <lunaix/types.h>
#include <stdint.h>

// 虽然内核不是进程，但为了区分，这里使用Pid=-1来指代内核。这主要是方便物理页所有权检查。
#define KERNEL_PID -1

#define PROC_STOPPED 0
#define PROC_RUNNING 1
#define PROC_TERMNAT 2
#define PROC_DESTROY 4
#define PROC_BLOCKED 8
#define PROC_CREATED 16

#define PROC_TERMMASK 0x6

struct proc_mm
{
    heap_context_t u_heap;
    struct mm_region* regions;
};

struct proc_sig
{
    void* signal_handler;
    int sig_num;
    isr_param prev_context;
};

#define PROC_SIG_SIZE sizeof(struct proc_sig) // size=84

struct proc_info
{
    /*
        Any change to *critical section*, including layout, size
        must be reflected in kernel/asm/x86/interrupt.S to avoid
        disaster!
     */

    /* ---- critical section start ---- */

    pid_t pid;
    struct proc_info* parent;
    isr_param intr_ctx; // size=76
    uintptr_t ustack_top;
    void* page_table;

    /* ---- critical section end ---- */

    struct llist_header siblings;
    struct llist_header children;
    struct llist_header grp_member;
    struct proc_mm mm;
    time_t created;
    uint8_t state;
    int32_t exit_code;
    int32_t k_status;
    sigset_t sig_pending;
    sigset_t sig_mask;
    void* sig_handler[_SIG_NUM];
    pid_t pgid;
    struct lx_timer* timer;
};

extern volatile struct proc_info* __current;

pid_t
alloc_pid();

void
init_proc(struct proc_info* pcb);

/**
 * @brief 向系统发布一个进程，使其可以被调度。
 *
 * @param process
 */
void
push_process(struct proc_info* process);

pid_t
destroy_process(pid_t pid);

void
setup_proc_mem(struct proc_info* proc, uintptr_t kstack_from);

/**
 * @brief 复制当前进程（LunaixOS的类 fork (unix) 实现）
 *
 */
pid_t
dup_proc();

/**
 * @brief 创建新进程（LunaixOS的类 CreateProcess (Windows) 实现）
 *
 */
void
new_proc();

/**
 * @brief 终止（退出）当前进程
 *
 */
void
terminate_proc(int exit_code);

int
orphaned_proc(pid_t pid);

struct proc_info*
get_process(pid_t pid);

#endif /* __LUNAIX_PROCESS_H */
