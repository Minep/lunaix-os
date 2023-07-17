#ifndef __LUNAIX_SIGNAL_H
#define __LUNAIX_SIGNAL_H

#include <usr/lunaix/signal_defs.h>

#define _SIG_NUM 16

#define _SIG_PENDING(bitmap, sig) ((bitmap) & (1 << (sig)))

#define _SIGSEGV SIGSEGV
#define _SIGALRM SIGALRM
#define _SIGCHLD SIGCHLD
#define _SIGCLD SIGCLD
#define _SIGINT SIGINT
#define _SIGKILL SIGKILL
#define _SIGSTOP SIGSTOP
#define _SIGCONT SIGCONT
#define _SIGTERM SIGTERM

#define __SIGNAL(num) (1 << (num))
#define __SIGSET(bitmap, num) (bitmap = bitmap | __SIGNAL(num))
#define __SIGTEST(bitmap, num) (bitmap & __SIGNAL(num))
#define __SIGCLEAR(bitmap, num) ((bitmap) = (bitmap) & ~__SIGNAL(num))

#define _SIGNAL_UNMASKABLE (__SIGNAL(_SIGKILL) | __SIGNAL(_SIGSTOP))

#define _SIG_BLOCK SIG_BLOCK
#define _SIG_UNBLOCK SIG_UNBLOCK
#define _SIG_SETMASK SIG_SETMASK

int
signal_send(pid_t pid, int signum);

#endif /* __LUNAIX_SIGNAL_H */
