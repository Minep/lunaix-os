#include <lunaix/syscall.h>
#include <signal.h>

__LXSYSCALL2(int, signal, int, signum, sighandler_t, handler);

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
