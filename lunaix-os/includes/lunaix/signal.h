#ifndef __LUNAIX_SIGNAL_H
#define __LUNAIX_SIGNAL_H

#define _SIG_NUM 8

#define _SIG_PENDING(bitmap, sig) ((bitmap) & (1 << (sig)))

#define _SIGSEGV 0
#define _SIGALRM 1
#define _SIGCHLD 2
#define _SIGCLD SIGCHLD
#define _SIGINT 3
#define _SIGKILL 4
#define _SIGSTOP 5
#define _SIGCONT 6

#define _SIGNAL_UNMASKABLE ((1 << _SIGKILL) | (1 << _SIGSTOP))

#define _SIG_BLOCK 1
#define _SIG_UNBLOCK 2
#define _SIG_SETMASK 3

typedef unsigned int sigset_t;
typedef void (*sighandler_t)(int);

void
signal_dispatch();

#endif /* __LUNAIX_SIGNAL_H */
