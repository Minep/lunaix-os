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

LOG_MODULE("SIG")

extern struct scheduler sched_ctx; /* kernel/sched.c */

#define UNMASKABLE (sigset(SIGKILL) | sigset(SIGTERM) | sigset(SIGILL))
#define TERMSIG (sigset(SIGSEGV) | sigset(SIGINT) | UNMASKABLE)
#define CORE (sigset(SIGSEGV))
#define within_kstack(addr)                                                    \
    (KERNEL_STACK <= (addr) && (addr) <= KERNEL_STACK_END)

static inline void
signal_terminate(int errcode)
{
    terminate_proc(errcode | PEXITSIG);
}

// Referenced in kernel/asm/x86/interrupt.S
void*
signal_dispatch()
{
    if (check_kcontext()) {
        // signal is undefined under 'kernel process'
        return 0;
    }

    if (!pending_sigs(__current)) {
        // 没有待处理信号
        return 0;
    }

    struct sigctx* psig = __current->sigctx;
    struct sigact* prev_working = active_signal(__current);
    sigset_t mask = psig->sig_mask | (prev_working ? prev_working->sa_mask : 0);

    int sig_selected = 31 - clz(psig->sig_pending & ~mask);
    sigset_clear(psig->sig_pending, sig_selected);

    if (!sig_selected) {
        // SIG0 is reserved
        return 0;
    }

    struct sigact* action = psig->signals[sig_selected];
    if (!action || !action->sa_actor) {
        if (sigset_test(TERMSIG, sig_selected)) {
            signal_terminate(sig_selected);
            schedule();
            // never return
        }
        return 0;
    }

    ptr_t ustack = __current->ustack_top;
    if ((int)(ustack - USR_STACK) < (int)sizeof(struct proc_sig)) {
        // 用户栈没有空间存放信号上下文
        return 0;
    }

    struct proc_sig* sigframe =
        (struct proc_sig*)((ustack - sizeof(struct proc_sig)) & ~0xf);

    sigframe->sig_num = sig_selected;
    sigframe->sigact = action->sa_actor;
    sigframe->sighand = action->sa_handler;

    sigframe->saved_ictx = __current->intr_ctx;

    sigactive_push(__current, sig_selected);

    return sigframe;
}

static inline void must_inline
__set_signal(struct proc_info* proc, signum_t signum) 
{
    raise_signal(proc, signum);
    
    struct sigact* sig = sigact_of(proc, signum);
    if (sig) {
        sig->sender = __current->pid;
    }
}

void
proc_setsignal(struct proc_info* proc, signum_t signum)
{
    __set_signal(proc, signum);
}

int
signal_send(pid_t pid, signum_t signum)
{
    if (signum >= _SIG_NUM) {
        __current->k_status = EINVAL;
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
        __current->k_status = EINVAL;
        return -1;
    }

send_grp: ;
    struct proc_info *pos, *n;
    llist_for_each(pos, n, &proc->grp_member, grp_member)
    {
        __set_signal(pos, signum);
    }

send_single:
    if (proc_terminated(proc)) {
        __current->k_status = EINVAL;
        return -1;
    }

    __set_signal(proc, signum);

    return 0;
}

void
signal_dup_context(struct sigctx* dest_ctx) 
{
    struct sigctx* old_ctx = __current->sigctx;

    memcpy(dest_ctx, old_ctx, sizeof(struct sigctx));

    for (int i = 0; i < _SIG_NUM; i++) {
        struct sigact* oldact = old_ctx->signals[i];
        if (!oldact) {
            continue;
        }
        
        struct sigact* newact = valloc(sizeof(struct sigact));
        memcpy(newact, oldact, sizeof(struct sigact));

        dest_ctx->signals[i] = newact;
    }
}

static inline void must_inline
signal_free_sigacts(struct sigctx* sigctx) {
    for (int i = 0; i < _SIG_NUM; i++) {
        struct sigact* act = sigctx->signals[i];
        if (act) {
            vfree(act);
        }
    }
} 

void
signal_reset_context(struct sigctx* sigctx) {
    signal_free_sigacts(sigctx);
    memset(sigctx, 0, sizeof(struct sigctx));
}

void
signal_free_context(struct sigctx* sigctx) {
    signal_free_sigacts(sigctx);
    vfree(sigctx);
}

__DEFINE_LXSYSCALL1(int, sigreturn, struct proc_sig, *sig_ctx)
{
    struct sigctx* sigctx = __current->sigctx;
    struct sigact* active = active_signal(__current);

    /* We choose signal#0 as our base case, that is sig#0 means no signal.
        Therefore, it is an ill situation to return from such sigctx.
    */
    if (!active) {
        signal_terminate(SIGSEGV);
        schedule();
    }

    __current->intr_ctx = sig_ctx->saved_ictx;
    if (proc_terminated(__current)) {
        __current->exit_code |= PEXITSIG;
    } else if (sigset_test(CORE, sig_ctx->sig_num)) {
        signal_terminate(sig_ctx->sig_num);
    }

    ptr_t ictx = (ptr_t)__current->intr_ctx;

    /*
        Ensure our restored context is within kernel stack

        This prevent user to forge their own context such that arbitrary code
       can be executed as supervisor level
    */
    if (!within_kstack(ictx)) {
        signal_terminate(SIGSEGV);
    }

    sigactive_pop(__current);

    schedule();

    // never reach!
    return 0;
}

__DEFINE_LXSYSCALL3(
    int, sigprocmask, int, how, const sigset_t, *set, sigset_t, *oldset)
{
    struct sigctx* sh = __current->sigctx;
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

    struct sigctx* sigctx = __current->sigctx;
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
    pause_current();
    sched_yieldk();

    __current->k_status = EINTR;
    return -1;
}

__DEFINE_LXSYSCALL2(int, kill, pid_t, pid, int, signum)
{
    return signal_send(pid, signum);
}

__DEFINE_LXSYSCALL1(int, sigpending, sigset_t, *sigset)
{
    *sigset = pending_sigs(__current);
    return 0;
}

__DEFINE_LXSYSCALL1(int, sigsuspend, sigset_t, *mask)
{
    sigset_t tmp = __current->sigctx->sig_mask;
    __current->sigctx->sig_mask = (*mask) & ~UNMASKABLE;

    pause_current();
    sched_yieldk();

    __current->sigctx->sig_mask = tmp;
    return -1;
}