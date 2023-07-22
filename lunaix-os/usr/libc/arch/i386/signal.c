#include "syscall.h"
#include <lunaix/signal_defs.h>
#include <lunaix/types.h>

__LXSYSCALL1(int, sigpending, sigset_t, *set);
__LXSYSCALL1(int, sigsuspend, const sigset_t, *mask);

__LXSYSCALL3(int,
             sigprocmask,
             int,
             how,
             const sigset_t,
             *set,
             sigset_t,
             *oldset);

__LXSYSCALL2(int, sys_sigaction, int, signum, struct sigaction*, action);

__LXSYSCALL2(int, kill, pid_t, pid, int, signum);

extern void
sigtrampoline();

sighandler_t
signal(int signum, sighandler_t handler)
{
    struct sigaction sa = { .sa_handler = (void*)handler,
                            .sa_mask = (sigset_t)-1,
                            .sa_sigaction = (void*)sigtrampoline };

    sys_sigaction(signum, &sa);

    return handler;
}

extern pid_t
getpid();

int
raise(int signum)
{
    return kill(getpid(), signum);
}

int
sigaction(int signum, struct sigaction* action)
{
    return sys_sigaction(signum, action);
}

struct siguctx
{
    void* sigact;
    void (*sa_handler)(int);
    unsigned char saved_frame[0];
};

void
sig_dohandling(int signum, void* siginfo, void* sigctx)
{
    struct siguctx* uctx = (struct siguctx*)sigctx;
    uctx->sa_handler(signum);
}