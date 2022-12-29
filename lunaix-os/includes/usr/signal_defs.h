#ifndef __LUNAIX_SYS_SIGNAL_DEFS_H
#define __LUNAIX_SYS_SIGNAL_DEFS_H

#define SIGSEGV 1
#define SIGALRM 2
#define SIGCHLD 3
#define SIGCLD SIGCHLD
#define SIGINT 4
#define SIGKILL 5
#define SIGSTOP 6
#define SIGCONT 7
#define SIGTERM 8

#define SIG_BLOCK 1
#define SIG_UNBLOCK 2
#define SIG_SETMASK 3

typedef unsigned int sigset_t;
typedef void (*sighandler_t)(int);

#endif /* __LUNAIX_SIGNAL_DEFS_H */
