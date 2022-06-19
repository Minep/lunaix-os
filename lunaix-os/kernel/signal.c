#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/signal.h>
#include <lunaix/syscall.h>

extern struct scheduler sched_ctx; /* kernel/sched.c */

void* default_handlers[_SIG_NUM] = {
    // TODO: 添加默认handler
};

// Referenced in kernel/asm/x86/interrupt.S
void*
signal_dispatch()
{
    // if (!(SEL_RPL(__current->intr_ctx.cs))) {
    //     // 同特权级间调度不进行信号处理
    //     return 0;
    // }

    if (!__current->sig_pending) {
        // 没有待处理信号
        return 0;
    }

    int sig_selected =
      31 - __builtin_clz(__current->sig_pending & ~__current->sig_mask);

    __current->sig_pending = __current->sig_pending & ~__SIGNAL(sig_selected);

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

    __current->sig_mask |= __SIGNAL(sig_selected);

    return sig_ctx;
}

__DEFINE_LXSYSCALL1(int, sigreturn, struct proc_sig, *sig_ctx)
{
    __current->intr_ctx = sig_ctx->prev_context;
    __current->sig_mask &= ~__SIGNAL(sig_ctx->sig_num);
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
    if (signum < 0 || signum >= _SIG_NUM) {
        return -1;
    }

    if (((1 << signum) & _SIGNAL_UNMASKABLE)) {
        return -1;
    }

    __current->sig_handler[signum] = (void*)handler;

    return 0;
}