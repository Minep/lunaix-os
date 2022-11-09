#ifndef __LUNAIX_PROCESS_H
#define __LUNAIX_PROCESS_H

#include <arch/x86/interrupts.h>
#include <lunaix/clock.h>
#include <lunaix/ds/waitq.h>
#include <lunaix/fs.h>
#include <lunaix/mm/mm.h>
#include <lunaix/signal.h>
#include <lunaix/timer.h>
#include <lunaix/types.h>
#include <stdint.h>

// 虽然内核不是进程，但为了区分，这里使用Pid=-1来指代内核。这主要是方便物理页所有权检查。
#define KERNEL_PID -1

#define PS_READY 0
#define PS_RUNNING 1
#define PS_TERMNAT 2
#define PS_DESTROY 4
#define PS_BLOCKED 8
#define PS_CREATED 16

#define PROC_TERMINATED(state) (state & 0x6)

#define PROC_FINPAUSE 1

struct proc_mm
{
    heap_context_t u_heap;
    struct mm_region regions;
};

struct proc_sigstate
{
    isr_param proc_regs;
    char fxstate[512] __attribute__((aligned(16)));
};

struct proc_sig
{
    void* signal_handler;
    int sig_num;
    struct proc_sigstate prev_context;
} __attribute__((packed));

struct proc_info
{
    /*
        Any change to *critical section*, including layout, size
        must be reflected in kernel/asm/x86/interrupt.S to avoid
        disaster!
     */

    /* ---- critical section start ---- */

    pid_t pid;                // offset = 0
    struct proc_info* parent; // offset = 4
    isr_param intr_ctx;       // offset = 8
    uintptr_t ustack_top;     // offset = 84
    void* page_table;         // offset = 88
    void* fxstate;            // offset = 92

    /* ---- critical section end ---- */

    struct llist_header tasks;
    struct llist_header siblings;
    struct llist_header children;
    struct llist_header grp_member;
    waitq_t waitqueue;

    struct
    {
        struct llist_header sleepers;
        time_t wakeup_time;
        time_t alarm_time;
    } sleep;

    struct proc_mm mm;
    time_t created;
    uint8_t state;
    int32_t exit_code;
    int32_t k_status;
    sigset_t sig_pending;
    sigset_t sig_mask;
    sigset_t sig_inprogress;
    int flags;
    void* sig_handler[_SIG_NUM];
    struct v_fdtable* fdtable;
    struct v_dnode* cwd;
    pid_t pgid;
};

extern volatile struct proc_info* __current;

static inline void
block_current()
{
    __current->state = PS_BLOCKED;
}

/**
 * @brief 分配并初始化一个进程控制块
 *
 * @return struct proc_info*
 */
struct proc_info*
alloc_process();

/**
 * @brief 初始化进程用户空间
 *
 * @param pcb
 */
void
init_proc_user_space(struct proc_info* pcb);

/**
 * @brief 向系统发布一个进程，使其可以被调度。
 *
 * @param process
 */
void
commit_process(struct proc_info* process);

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
