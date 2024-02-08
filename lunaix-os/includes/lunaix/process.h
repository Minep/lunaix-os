#ifndef __LUNAIX_PROCESS_H
#define __LUNAIX_PROCESS_H

#include <lunaix/clock.h>
#include <lunaix/ds/waitq.h>
#include <lunaix/fs.h>
#include <lunaix/iopoll.h>
#include <lunaix/mm/mm.h>
#include <lunaix/mm/pagetable.h>
#include <lunaix/mm/region.h>
#include <lunaix/signal.h>
#include <lunaix/timer.h>
#include <lunaix/types.h>
#include <lunaix/spike.h>
#include <lunaix/pcontext.h>
#include <stdint.h>


/*
    |C|Sp|Bk|De|Tn|Pu|Rn|
            \----/
              Dt

    Group Dt: whether this process is terminated.

    Rn: Running
    Tn: Terminated
    De: Destoryed
    Pu: Paused
    Bk: Blocked
    Sp: Stopped
    C : Created
*/

#define PS_READY 0
#define PS_RUNNING 1
#define PS_TERMNAT 2
#define PS_DESTROY 4
#define PS_PAUSED 8
#define PS_BLOCKED 16
#define PS_STOPPED 32
#define PS_CREATED 64

#define PS_GrBP (PS_PAUSED | PS_BLOCKED | PS_STOPPED)
#define PS_GrDT (PS_TERMNAT | PS_DESTROY)
#define PS_Rn (PS_RUNNING | PS_CREATED)

#define proc_terminated(proc) (((proc)->state) & PS_GrDT)
#define proc_hanged(proc) (((proc)->state) & PS_BLOCKED)
#define proc_runnable(proc) (!(proc)->state || !(((proc)->state) & ~PS_Rn))


#define TH_DETACHED 0b0001

#define thread_detached(th) ((th)->flags & TH_DETACHED)
#define detach_thread(th) ((th)->flags |= TH_DETACHED)

struct proc_sig
{
    int sig_num;
    void* sigact;
    void* sighand;
    isr_param* saved_ictx;
} __attribute__((packed));


struct proc_info;

struct haybed {
    struct llist_header sleepers;
    time_t wakeup_time;
    time_t alarm_time;
};

struct thread
{
    /*
        Any change to *critical section*, including layout, size
        must be reflected in arch/i386/interrupt.S.inc to avoid
        disaster!
     */
    struct
    {
        isr_param* intr_ctx;
        ptr_t ustack_top;
    };                              // *critical section

    struct {
        tid_t tid;
        time_t created;
        int state;
        int syscall_ret;
        ptr_t exit_val;
        int flags;
    };

    struct {
        ptr_t kstack;               // process local kernel stack
        struct mm_region* ustack;   // process local user stack (NULL for kernel thread)
    };

    struct haybed sleep;

    struct proc_info* process;
    struct llist_header proc_sibs;  // sibling to process-local threads
    struct llist_header sched_sibs; // sibling to scheduler (global) threads
    struct sigctx sigctx;
    waitq_t waitqueue;
};

struct proc_info
{
    // active thread, must be at the very beginning
    struct thread* th_active;

    struct llist_header threads;
    int thread_count;

    struct llist_header tasks;

    struct llist_header siblings;
    struct llist_header children;
    struct llist_header grp_member;

    struct {
        struct proc_info* parent;
        pid_t pid;
        pid_t pgid;
        time_t created;

        int state;
        int exit_code;
    };

    struct proc_mm* mm;
    struct sigregister* sigreg;
    struct v_fdtable* fdtable;
    struct v_dnode* cwd;
    struct {
        char* cmd;
        size_t cmd_len;
    };

    struct iopoll pollctx;
};

extern volatile struct proc_info* __current;
extern volatile struct thread* current_thread;

/**
 * @brief Check if current process belong to kernel itself
 * (pid=0)
 */
#define kernel_process(proc) (!(proc)->pid)

#define resume_thread(th) (th)->state = PS_READY
#define pause_thread(th) (th)->state = PS_PAUSED
#define block_thread(th) (th)->state = PS_BLOCKED

static inline void must_inline
set_current_executing(struct thread* thread)
{
    current_thread = thread;
    __current = thread->process;
}

static inline struct proc_mm* 
vmspace(struct proc_info* proc) 
{
    return proc->mm;
}

static inline ptr_t
vmroot(struct proc_info* proc) 
{
    return proc->mm->vmroot;
}

static inline vm_regions_t* 
vmregions(struct proc_info* proc) 
{
    return &proc->mm->regions;
}

static inline void
block_current_thread()
{
    block_thread(current_thread);
}

static inline void
pause_current_thread()
{
    pause_thread(current_thread);
}

static inline void
resume_current_thread()
{
    resume_thread(current_thread);
}

static inline int syscall_result(int retval) {
    return (current_thread->syscall_ret = retval);
}

/**
 * @brief Spawn a process with arbitary entry point. 
 *        The inherit priviledge level is deduced automatically
 *        from the given entry point
 * 
 * @param created returned created main thread
 * @param entry entry point
 * @param with_ustack whether to pre-allocate a user stack with it
 * @return int 
 */
int
spawn_process(struct thread** created, ptr_t entry, bool with_ustack);

/**
 * @brief Spawn a process that housing a given executable image as well as 
 *        program argument and environment setting
 * 
 * @param created returned created main thread
 * @param path file system path to executable
 * @param argv arguments passed to executable
 * @param envp environment variables passed to executable
 * @return int 
 */
int
spawn_process_usr(struct thread** created, char* path, 
                    const char** argv, const char** envp);

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
delete_process(struct proc_info* proc);

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
terminate_current(int exit_code);

void 
terminate_proccess(struct proc_info* proc, int exit_code);

int
orphaned_proc(pid_t pid);

struct proc_info*
get_process(pid_t pid);

/* 
    ========= Thread =========
*/

void
commit_thread(struct thread* thread);

struct thread*
alloc_thread(struct proc_info* process);

void
destory_thread(ptr_t vm_mnt, struct thread* thread);

void
terminate_thread(struct thread* thread, ptr_t val);

void
terminate_current_thread(ptr_t val);

struct thread*
create_thread(struct proc_info* proc, ptr_t vm_mnt, bool with_ustack);

void
start_thread(struct thread* th, ptr_t vm_mnt, ptr_t entry);

static inline void
spawn_kthread(ptr_t entry) {
    assert(kernel_process(__current));

    struct thread* th = create_thread(__current, VMS_SELF, false);
    
    assert(th);
    start_thread(th, VMS_SELF, entry);
}

void 
exit_thread(void* val);

void
thread_release_mem(struct thread* thread, ptr_t vm_mnt);

/* 
    ========= Signal =========
*/

#define pending_sigs(thread) ((thread)->sigctx.sig_pending)
#define raise_signal(thread, sig) sigset_add(pending_sigs(thread), sig)
#define sigact_of(proc, sig) ((proc)->sigreg->signals[(sig)])
#define set_sigact(proc, sig, sigact) ((proc)->sigreg->signals[(sig)] = (sigact))

static inline struct sigact*
active_signal(struct thread* thread) {
    struct sigctx* sigctx = &thread->sigctx;
    struct sigregister* sigreg = thread->process->sigreg;
    return sigreg->signals[sigctx->sig_active];
} 

static inline void 
sigactive_push(struct thread* thread, int active_sig) {
    struct sigctx* sigctx = &thread->sigctx;
    int prev_active = sigctx->sig_active;

    assert(sigact_of(thread->process, active_sig));

    sigctx->sig_order[active_sig] = prev_active;
    sigctx->sig_active = active_sig;
}

static inline void 
sigactive_pop(struct thread* thread) {
    struct sigctx* sigctx = &thread->sigctx;
    int active_sig = sigctx->sig_active;

    sigctx->sig_active = sigctx->sig_order[active_sig];
    sigctx->sig_order[active_sig] = active_sig;
}

void
proc_setsignal(struct proc_info* proc, signum_t signum);

void
thread_setsignal(struct thread* thread, signum_t signum);


#endif /* __LUNAIX_PROCESS_H */
