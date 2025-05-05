#include <syscall.h>
#include <lunaix/signal.h>
#include <sys/types.h>

int
sigpending(sigset_t *set)
{
    return do_lunaix_syscall(__NR__lxsys_sigpending, set);
}

int
sigsuspend(const sigset_t *mask)
{
    return do_lunaix_syscall(__NR__lxsys_sigsuspend, mask);
}

int
sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    return do_lunaix_syscall(__NR__lxsys_sigprocmask, how, set, oldset);
}

int
sys_sigaction(int signum, struct sigaction* action)
{
    return do_lunaix_syscall(__NR__lxsys_sys_sigaction, signum, action);
}

int
kill(pid_t pid, int signum)
{
    return do_lunaix_syscall(__NR__lxsys_kill, pid, signum);
}

extern void
sigtrampoline();

extern pid_t
getpid();

sighandler_t
signal(int signum, sighandler_t handler)
{
    struct sigaction sa = { .sa_handler = (void*)handler,
                            .sa_mask = (sigset_t)-1,
                            .sa_sigaction = (void*)sigtrampoline };

    sys_sigaction(signum, &sa);

    return handler;
}

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