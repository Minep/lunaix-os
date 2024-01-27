#ifndef __LUNAIX_PROCESS_H
#define __LUNAIX_PROCESS_H

#include <lunaix/clock.h>
#include <lunaix/ds/waitq.h>
#include <lunaix/fs.h>
#include <lunaix/iopoll.h>
#include <lunaix/mm/mm.h>
#include <lunaix/mm/region.h>
#include <lunaix/signal.h>
#include <lunaix/timer.h>
#include <lunaix/types.h>
#include <lunaix/spike.h>
#include <stdint.h>
#include <sys/interrupts.h>

/*
    Although kernel is not a process, it is rather useful given that 
    we might introduce concept of "kernel thread" in the future thus we can 
    integrate some periodical, non-crucial kernel task into scheduling 
    subsystem. (E.g., Linux's khugepaged)
*/
#define KERNEL_PID -1

/*
    |C|Bk|De|Tn|Pu|Rn|
         \----/
            Dt

    Group Dt: whether this process is terminated.

    Rn: Running
    Tn: Terminated
    De: Destoryed
    Pu: Paused
    Bk: Blocked
    C : Created
*/

#define PS_READY 0
#define PS_RUNNING 1
#define PS_TERMNAT 2
#define PS_DESTROY 4
#define PS_PAUSED 8
#define PS_BLOCKED 16
#define PS_CREATED 32

#define PS_GrBP (PS_PAUSED | PS_BLOCKED)
#define PS_GrDT (PS_TERMNAT | PS_DESTROY)

#define proc_terminated(proc) (((proc)->state) & PS_GrDT)
#define proc_hanged(proc) (((proc)->state) & PS_BLOCKED)
#define proc_runnable(proc) (((proc)->state) & PS_PAUSED)

struct proc_sig
{
    int sig_num;
    void* sigact;
    void* sighand;
    isr_param* saved_ictx;
} __attribute__((packed));

struct proc_info
{
    /*
        Any change to *critical section*, including layout, size
        must be reflected in arch/i386/interrupt.S.inc to avoid
        disaster!
     */

    /* ---- critical section start ---- */

    struct
    {
        isr_param* intr_ctx;
        ptr_t ustack_top;
        ptr_t page_table;
    };

    /* ---- critical section end ---- */

    struct llist_header tasks;
    struct llist_header siblings;
    struct llist_header children;
    struct llist_header grp_member;
    waitq_t waitqueue;

    struct {
        struct proc_info* parent;
        pid_t pid;
        pid_t pgid;
        time_t created;
        u8_t state;
        int32_t exit_code;
        int32_t k_status;
    };

    struct
    {
        struct llist_header sleepers;
        time_t wakeup_time;
        time_t alarm_time;
    } sleep;

    struct proc_mm mm;
    struct sigctx* sigctx;
    struct v_fdtable* fdtable;
    struct v_dnode* cwd;

    struct iopoll pollctx;
};
extern volatile struct proc_info* __current;

#define check_kcontext() (__current->pid == KERNEL_PID)

#define resume_process(proc) (proc)->state = PS_READY
#define pause_process(proc) (proc)->state = PS_PAUSED
#define block_process(proc) (proc)->state = PS_BLOCKED

static inline void
block_current()
{
    block_process(__current);
}

static inline void
pause_current()
{
    pause_process(__current);
}

static inline void
resume_current()
{
    resume_process(__current);
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
copy_kernel_stack(struct proc_info* proc, ptr_t kstack_from);

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

// enable interrupt upon transfer
#define TRANSFER_IE 1

/**
 * @brief Setup process initial context, used to initiate first switch
 *
 * @param proc
 * @param stop
 * @param target
 * @param flags
 */
void
proc_init_transfer(struct proc_info* proc, ptr_t stop, ptr_t target, int flags);

#define pending_sigs(proc) ((proc)->sigctx->sig_pending)
#define raise_signal(proc, sig) sigset_add(pending_sigs(proc), sig)
#define sigact_of(proc, sig) ((proc)->sigctx->signals[(sig)])
#define set_sigact(proc, sig, sigact) ((proc)->sigctx->signals[(sig)] = (sigact))

static inline struct sigact*
active_signal(struct proc_info* proc) {
    struct sigctx* sigctx = (proc)->sigctx;
    return sigctx->signals[sigctx->sig_active];
} 

static inline void 
sigactive_push(struct proc_info* proc, int active_sig) {
    struct sigctx* sigctx = proc->sigctx;
    int prev_active = sigctx->sig_active;

    assert(sigctx->signals[active_sig]);

    sigctx->sig_order[active_sig] = prev_active;
    sigctx->sig_active = active_sig;
}

static inline void 
sigactive_pop(struct proc_info* proc) {
    struct sigctx* sigctx = proc->sigctx;
    int active_sig = sigctx->sig_active;

    sigctx->sig_active = sigctx->sig_order[active_sig];
    sigctx->sig_order[active_sig] = active_sig;
}

void
proc_setsignal(struct proc_info* proc, signum_t signum);


#endif /* __LUNAIX_PROCESS_H */
