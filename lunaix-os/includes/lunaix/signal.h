#ifndef __LUNAIX_SIGNAL_H
#define __LUNAIX_SIGNAL_H

#include <lunaix/syscall.h>

#define _SIG_NUM 16

#define _SIG_PENDING(bitmap, sig) ((bitmap) & (1 << (sig)))

#define _SIGSEGV 1
#define _SIGALRM 2
#define _SIGCHLD 3
#define _SIGCLD _SIGCHLD
#define _SIGINT 4
#define _SIGKILL 5
#define _SIGSTOP 6
#define _SIGCONT 7
#define _SIGTERM 8

#define __SIGNAL(num) (1 << (num))
#define __SIGSET(bitmap, num) (bitmap = bitmap | __SIGNAL(num))
#define __SIGTEST(bitmap, num) (bitmap & __SIGNAL(num))
#define __SIGCLEAR(bitmap, num) ((bitmap) = (bitmap) & ~__SIGNAL(num))

#define _SIGNAL_UNMASKABLE (__SIGNAL(_SIGKILL) | __SIGNAL(_SIGSTOP))

#define _SIG_BLOCK 1
#define _SIG_UNBLOCK 2
#define _SIG_SETMASK 3

typedef unsigned int sigset_t;
typedef void (*sighandler_t)(int);

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

#endif /* __LUNAIX_SIGNAL_H */
