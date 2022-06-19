#ifndef __LUNAIX_SIGNAL_H
#define __LUNAIX_SIGNAL_H

#include <lunaix/syscall.h>

#define _SIG_NUM 8

#define _SIG_PENDING(bitmap, sig) ((bitmap) & (1 << (sig)))

#define _SIGSEGV 0
#define _SIGALRM 1
#define _SIGCHLD 2
#define _SIGCLD _SIGCHLD
#define _SIGINT 3
#define _SIGKILL 4
#define _SIGSTOP 5
#define _SIGCONT 6

#define __SIGNAL(num) (1 << (num))
#define __SET_SIGNAL(bitmap, num) (bitmap = bitmap | __SIGNAL(num))

#define _SIGNAL_UNMASKABLE (__SIGNAL(_SIGKILL) | __SIGNAL(_SIGSTOP))

#define _SIG_BLOCK 1
#define _SIG_UNBLOCK 2
#define _SIG_SETMASK 3

typedef unsigned int sigset_t;
typedef void (*sighandler_t)(int);

__LXSYSCALL2(int, signal, int, signum, sighandler_t, handler);

#endif /* __LUNAIX_SIGNAL_H */
