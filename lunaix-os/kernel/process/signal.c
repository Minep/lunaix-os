#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/signal.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syscall.h>
#include <lunaix/syslog.h>
#include <lunaix/mm/valloc.h>

#include <klibc/string.h>

#include <sys/mm/mempart.h>

// FIXME issues with signal

LOG_MODULE("SIG")

extern struct scheduler sched_ctx; /* kernel/sched.c */

#define UNMASKABLE (sigset(SIGKILL) | sigset(SIGTERM) | sigset(SIGILL))
#define TERMSIG (sigset(SIGSEGV) | sigset(SIGINT) | UNMASKABLE)
#define CORE (sigset(SIGSEGV))
#define within_kstack(addr)                                                    \
    (KSTACK_AREA <= (addr) && (addr) <= KSTACK_AREA_END)

static inline void
signal_terminate(int caused_by)
{
    terminate_current(caused_by | PEXITSIG);
}

// Referenced in kernel/asm/x86/interrupt.S
void*
signal_dispatch()
{
    if (kernel_process(__current)) {
        // signal is undefined under 'kernel process'
        return 0;
    }

    if (!pending_sigs(current_thread)) {
        // 没有待处理信号
        return 0;
    }

    struct sigregister* sigreg = __current->sigreg;
    struct sigctx* psig = &current_thread->sigctx;
    struct sigact* prev_working = active_signal(current_thread);
    sigset_t mask = psig->sig_mask | (prev_working ? prev_working->sa_mask : 0);

    int sig_selected = 31 - clz(psig->sig_pending & ~mask);
    sigset_clear(psig->sig_pending, sig_selected);

    if (!sig_selected) {
        // SIG0 is reserved
        return 0;
    }

    struct sigact* action = sigreg->signals[sig_selected];
    if (!action || !action->sa_actor) {
        if (sigset_test(TERMSIG, sig_selected)) {
            signal_terminate(sig_selected);
            schedule();
            // never return
        }
        return 0;
    }

    ptr_t ustack = current_thread->ustack_top;
    if ((int)(ustack - USR_STACK) < (int)sizeof(struct proc_sig)) {
        // 用户栈没有空间存放信号上下文
        return 0;
    }

    struct proc_sig* sigframe =
        (struct proc_sig*)((ustack - sizeof(struct proc_sig)) & ~0xf);

    sigframe->sig_num = sig_selected;
    sigframe->sigact = action->sa_actor;
    sigframe->sighand = action->sa_handler;

    sigframe->saved_ictx = current_thread->intr_ctx;

    sigactive_push(current_thread, sig_selected);

    return sigframe;
}

static inline void must_inline
__set_signal(struct thread* thread, signum_t signum) 
{
    raise_signal(thread, signum);

    // for these mutually exclusive signal
    if (signum == SIGCONT || signum == SIGSTOP) {
        sigset_clear(thread->sigctx.sig_pending, signum ^ 1);
    }
    
    struct sigact* sig = sigact_of(thread->process, signum);
    if (sig) {
        sig->sender = __current->pid;
    }
}

static inline void must_inline
__set_signal_all_threads(struct proc_info* proc, signum_t signum) 
{
    struct thread *pos, *n;
    llist_for_each(pos, n, &proc->threads, proc_sibs) {
        __set_signal(pos, signum);
    }
}

void
thread_setsignal(struct thread* thread, signum_t signum)
{
    if (unlikely(kernel_process(thread->process))) {
        return;
    }

    __set_signal(thread, signum);
}

void
proc_setsignal(struct proc_info* proc, signum_t signum)
{
    if (unlikely(kernel_process(proc))) {
        return;
    }

    // FIXME handle signal delivery at process level.
    switch (signum)
    {
    case SIGKILL:
        signal_terminate(signum);
        break;
    case SIGCONT:
    case SIGSTOP:
        __set_signal_all_threads(proc, signum);
    default:
        break;
    }
    
    __set_signal(proc->th_active, signum);
}

int
signal_send(pid_t pid, signum_t signum)
{
    if (signum >= _SIG_NUM) {
        syscall_result(EINVAL);
        return -1;
    }

    pid_t sender_pid = __current->pid;
    struct proc_info* proc;

    if (pid > 0) {
        proc = get_process(pid);
        goto send_single;
    } else if (!pid) {
        proc = __current;
        goto send_grp;
    } else if (pid < 0) {
        proc = get_process(-pid);
        goto send_grp;
    } else {
        // TODO: send to all process.
        //  But I don't want to support it yet.
        syscall_result(EINVAL);
        return -1;
    }

send_grp: ;
    struct proc_info *pos, *n;
    llist_for_each(pos, n, &proc->grp_member, grp_member)
    {
        proc_setsignal(pos, signum);
    }

send_single:
    if (proc_terminated(proc)) {
        syscall_result(EINVAL);
        return -1;
    }

    proc_setsignal(proc, signum);

    return 0;
}

void
signal_dup_context(struct sigctx* dest_ctx) 
{
    struct sigctx* old_ctx = &current_thread->sigctx;
    memcpy(dest_ctx, old_ctx, sizeof(struct sigctx));
}

void
signal_dup_registers(struct sigregister* dest_reg)
{
    struct sigregister* oldreg = __current->sigreg;
    for (int i = 0; i < _SIG_NUM; i++) {
        struct sigact* oldact = oldreg->signals[i];
        if (!oldact) {
            continue;
        }
        
        struct sigact* newact = valloc(sizeof(struct sigact));
        memcpy(newact, oldact, sizeof(struct sigact));

        dest_reg->signals[i] = newact;
    }
}

void
signal_reset_context(struct sigctx* sigctx) {
    memset(sigctx, 0, sizeof(struct sigctx));
}

void
signal_reset_register(struct sigregister* sigreg) {
    for (int i = 0; i < _SIG_NUM; i++) {
        struct sigact* act = sigreg->signals[i];
        if (act) {
            vfree(act);
            sigreg->signals[i] = NULL;
        }
    }
}

void
signal_free_registers(struct sigregister* sigreg) {
    signal_reset_register(sigreg);
    vfree(sigreg);
}

__DEFINE_LXSYSCALL1(int, sigreturn, struct proc_sig, *sig_ctx)
{
    struct sigctx* sigctx = &current_thread->sigctx;
    struct sigact* active = active_signal(current_thread);

    /* We choose signal#0 as our base case, that is sig#0 means no signal.
        Therefore, it is an ill situation to return from such sigctx.
    */
    if (!active) {
        signal_terminate(SIGSEGV);
        schedule();
    }

    current_thread->intr_ctx = sig_ctx->saved_ictx;
    if (proc_terminated(__current)) {
        __current->exit_code |= PEXITSIG;
    } else if (sigset_test(CORE, sig_ctx->sig_num)) {
        signal_terminate(sig_ctx->sig_num);
    }

    ptr_t ictx = (ptr_t)current_thread->intr_ctx;

    /*
        Ensure our restored context is within kernel stack

        This prevent user to forge their own context such that arbitrary code
       can be executed as supervisor level
    */
    if (!within_kstack(ictx)) {
        signal_terminate(SIGSEGV);
    }

    sigactive_pop(current_thread);

    schedule();

    // never reach!
    return 0;
}

__DEFINE_LXSYSCALL3(
    int, sigprocmask, int, how, const sigset_t, *set, sigset_t, *oldset)
{
    struct sigctx* sh = &current_thread->sigctx;
    *oldset = sh->sig_mask;

    if (how == _SIG_BLOCK) {
        sigset_union(sh->sig_mask, *set);
    } else if (how == _SIG_UNBLOCK) {
        sigset_intersect(sh->sig_mask, ~(*set));
    } else if (how == _SIG_SETMASK) {
        sh->sig_mask = *set;
    } else {
        return 0;
    }

    sigset_intersect(sh->sig_mask, ~UNMASKABLE);
    return 1;
}

__DEFINE_LXSYSCALL2(int, sys_sigaction, int, signum, struct sigaction*, action)
{
    if (signum <= 0 || signum >= _SIG_NUM) {
        return -1;
    }

    if (sigset_test(UNMASKABLE, signum)) {
        return -1;
    }

    struct sigctx* sigctx = &current_thread->sigctx;
    if (signum == sigctx->sig_active) {
        return -1;
    }

    struct sigact* sa = sigact_of(__current, signum);

    if (!sa) {
        sa = vzalloc(sizeof(struct sigact));
        set_sigact(__current, signum, sa);
    }

    sa->sa_actor = (void*)action->sa_sigaction;
    sa->sa_handler = (void*)action->sa_handler;
    sigset_union(sa->sa_mask, sigset(signum));

    return 0;
}

__DEFINE_LXSYSCALL(int, pause)
{
    pause_current_thread();
    sched_pass();

    syscall_result(EINTR);
    return -1;
}

__DEFINE_LXSYSCALL2(int, kill, pid_t, pid, int, signum)
{
    return signal_send(pid, signum);
}

__DEFINE_LXSYSCALL1(int, sigpending, sigset_t, *sigset)
{
    *sigset = pending_sigs(current_thread);
    return 0;
}

__DEFINE_LXSYSCALL1(int, sigsuspend, sigset_t, *mask)
{
    struct sigctx* sigctx = &current_thread->sigctx;
    sigset_t tmp = current_thread->sigctx.sig_mask;
    sigctx->sig_mask = (*mask) & ~UNMASKABLE;

    pause_current_thread();
    sched_pass();

    sigctx->sig_mask = tmp;
    return -1;
}