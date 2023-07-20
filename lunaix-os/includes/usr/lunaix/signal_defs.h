#ifndef __LUNAIX_SYS_SIGNAL_DEFS_H
#define __LUNAIX_SYS_SIGNAL_DEFS_H

#define SIGALRM 1
#define SIGCHLD 2
#define SIGCLD SIGCHLD

#define SIGSTOP 3
#define SIGCONT 4

#define SIGINT 5
#define SIGSEGV 6
#define SIGKILL 7
#define SIGTERM 8

#define SIG_BLOCK 1
#define SIG_UNBLOCK 2
#define SIG_SETMASK 3

typedef unsigned int sigset_t;
typedef void (*sighandler_t)(int);

#endif /* __LUNAIX_SIGNAL_DEFS_H */
