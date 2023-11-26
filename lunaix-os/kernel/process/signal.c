#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/signal.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syscall.h>
#include <lunaix/syslog.h>

#include <klibc/string.h>

#include <sys/mm/mempart.h>

LOG_MODULE("SIG")

extern struct scheduler sched_ctx; /* kernel/sched.c */

#define UNMASKABLE (sigset(SIGKILL) | sigset(SIGTERM))
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
    if (!__current->sigctx.sig_pending) {
        // 没有待处理信号
        return 0;
    }

    struct sighail* psig = &__current->sigctx;
    struct sigact* prev_working = psig->inprogress;
    sigset_t mask = psig->sig_mask | (prev_working ? prev_working->sa_mask : 0);

    int sig_selected = 31 - __builtin_clz(psig->sig_pending & ~mask);

    sigset_clear(psig->sig_pending, sig_selected);

    struct sigact* action = &psig->signals[sig_selected];

    if (sig_selected == 0) {
        // SIG0 is reserved
        return 0;
    }

    if (!action->sa_actor) {
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

    action->prev = prev_working;
    psig->inprogress = action;

    return sigframe;
}

void
proc_clear_signal(struct proc_info* proc)
{
    memset(&proc->sigctx, 0, sizeof(proc->sigctx));
}

void
proc_setsignal(struct proc_info* proc, int signum)
{
    sigset_add(proc->sigctx.sig_pending, signum);
    proc->sigctx.signals[signum].sender = __current->pid;
}

int
signal_send(pid_t pid, int signum)
{
    if (signum < 0 || signum >= _SIG_NUM) {
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

send_grp:
    struct proc_info *pos, *n;
    llist_for_each(pos, n, &proc->grp_member, grp_member)
    {
        struct sighail* sh = &pos->sigctx;
        sigset_add(sh->sig_pending, signum);
        sh->signals[signum].sender = sender_pid;
    }

send_single:
    if (proc_terminated(proc)) {
        __current->k_status = EINVAL;
        return -1;
    }

    sigset_add(proc->sigctx.sig_pending, signum);
    proc->sigctx.signals[signum].sender = sender_pid;

    return 0;
}

__DEFINE_LXSYSCALL1(int, sigreturn, struct proc_sig, *sig_ctx)
{
    __current->intr_ctx = sig_ctx->saved_ictx;

    struct sigact* current = __current->sigctx.inprogress;
    if (current) {
        __current->sigctx.inprogress = current->prev;
        current->prev = NULL;
    } else {
        __current->sigctx.inprogress = NULL;
    }

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

    schedule();

    // never reach!
    return 0;
}

__DEFINE_LXSYSCALL3(int,
                    sigprocmask,
                    int,
                    how,
                    const sigset_t,
                    *set,
                    sigset_t,
                    *oldset)
{
    struct sighail* sh = &__current->sigctx;
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

    struct sigact* sa = &__current->sigctx.signals[signum];

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
    *sigset = __current->sigctx.sig_pending;
    return 0;
}

__DEFINE_LXSYSCALL1(int, sigsuspend, sigset_t, *mask)
{
    sigset_t tmp = __current->sigctx.sig_mask;
    __current->sigctx.sig_mask = (*mask) & ~UNMASKABLE;

    pause_current();
    sched_yieldk();

    __current->sigctx.sig_mask = tmp;
    return -1;
}