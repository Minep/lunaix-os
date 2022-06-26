#include <lunaix/lunistd.h>
#include <lunaix/lxsignal.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/signal.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syscall.h>

extern struct scheduler sched_ctx; /* kernel/sched.c */

void __USER__
default_sighandler_term(int signum)
{
    _exit(signum);
}

void* default_handlers[_SIG_NUM] = {
    // TODO: 添加默认handler
    [_SIGINT] = default_sighandler_term,  [_SIGTERM] = default_sighandler_term,
    [_SIGKILL] = default_sighandler_term, [_SIGSEGV] = default_sighandler_term,
    [_SIGINT] = default_sighandler_term,
};

// Referenced in kernel/asm/x86/interrupt.S
void*
signal_dispatch()
{
    if (!__current->sig_pending) {
        // 没有待处理信号
        return 0;
    }

    int sig_selected =
      31 - __builtin_clz(__current->sig_pending &
                         ~(__current->sig_mask | __current->sig_inprogress));

    __SIGCLEAR(__current->sig_pending, sig_selected);

    if (sig_selected == 0) {
        // SIG0 is reserved
        return 0;
    }

    if (!__current->sig_handler[sig_selected] &&
        !default_handlers[sig_selected]) {
        // 如果该信号没有handler，则忽略
        return 0;
    }

    uintptr_t ustack = __current->ustack_top & ~0xf;

    if ((int)(ustack - USTACK_END) < (int)sizeof(struct proc_sig)) {
        // 用户栈没有空间存放信号上下文
        return 0;
    }

    struct proc_sig* sig_ctx =
      (struct proc_sig*)(ustack - sizeof(struct proc_sig));

    sig_ctx->prev_context = __current->intr_ctx;
    sig_ctx->sig_num = sig_selected;
    sig_ctx->signal_handler = __current->sig_handler[sig_selected];

    if (!sig_ctx->signal_handler) {
        // 如果没有用户自定义的Handler，则使用系统默认Handler。
        sig_ctx->signal_handler = default_handlers[sig_selected];
    }

    __SIGSET(__current->sig_inprogress, sig_selected);

    return sig_ctx;
}

int
signal_send(pid_t pid, int signum)
{
    if (signum < 0 || signum >= _SIG_NUM) {
        __current->k_status = LXINVL;
        return -1;
    }

    struct proc_info* proc;
    if (pid > 0) {
        proc = get_process(pid);
        goto send_single;
    } else if (!pid) {
        proc = __current;
        goto send_grp;
    } else if (pid < -1) {
        proc = get_process(-pid);
        goto send_grp;
    } else {
        // TODO: send to all process.
        //  But I don't want to support it yet.
        __current->k_status = LXINVL;
        return -1;
    }

send_grp:
    struct proc_info *pos, *n;
    llist_for_each(pos, n, &proc->grp_member, grp_member)
    {
        __SIGSET(pos->sig_pending, signum);
    }
    return 0;

send_single:
    if (PROC_TERMINATED(proc->state)) {
        __current->k_status = LXINVL;
        return -1;
    }
    __SIGSET(proc->sig_pending, signum);
    return 0;
}

__DEFINE_LXSYSCALL1(int, sigreturn, struct proc_sig, *sig_ctx)
{
    __current->intr_ctx = sig_ctx->prev_context;
    __current->flags &= ~PROC_FINPAUSE;
    __SIGCLEAR(__current->sig_inprogress, sig_ctx->sig_num);
    schedule();
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
    *oldset = __current->sig_mask;
    if (how == _SIG_BLOCK) {
        __current->sig_mask |= *set;
    } else if (how == _SIG_UNBLOCK) {
        __current->sig_mask &= ~(*set);
    } else if (how == _SIG_SETMASK) {
        __current->sig_mask = *set;
    } else {
        return 0;
    }
    __current->sig_mask &= ~_SIGNAL_UNMASKABLE;
    return 1;
}

__DEFINE_LXSYSCALL2(int, signal, int, signum, sighandler_t, handler)
{
    if (signum <= 0 || signum >= _SIG_NUM) {
        return -1;
    }

    if ((__SIGNAL(signum) & _SIGNAL_UNMASKABLE)) {
        return -1;
    }

    __current->sig_handler[signum] = (void*)handler;

    return 0;
}

void
__do_pause()
{
    __current->flags |= PROC_FINPAUSE;

    __SYSCALL_INTERRUPTIBLE({
        while ((__current->flags & PROC_FINPAUSE)) {
            sched_yield();
        }
    })
    __current->k_status = EINTR;
}

__DEFINE_LXSYSCALL(int, pause)
{
    __do_pause();
    return -1;
}

__DEFINE_LXSYSCALL2(int, kill, pid_t, pid, int, signum)
{
    return signal_send(pid, signum);
}

__DEFINE_LXSYSCALL1(int, sigpending, sigset_t, *sigset)
{
    *sigset = __current->sig_pending;
    return 0;
}

__DEFINE_LXSYSCALL1(int, sigsuspend, sigset_t, *mask)
{
    sigset_t tmp = __current->sig_mask;
    __current->sig_mask = (*mask) & ~_SIGNAL_UNMASKABLE;
    __do_pause();
    __current->sig_mask = tmp;
    return -1;
}