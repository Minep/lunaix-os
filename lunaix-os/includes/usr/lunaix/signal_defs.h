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
#define SIGILL 9
#define SIGSYS 10

#define SIG_BLOCK 1
#define SIG_UNBLOCK 2
#define SIG_SETMASK 3

typedef unsigned char signum_t;
typedef unsigned int sigset_t;
typedef void (*sighandler_t)(int);

struct sigaction
{
    sigset_t sa_mask;
    void (*sa_handler)(int);
    void (*sa_sigaction)(int, void*, void*);
};

struct siginfo
{
    // TODO
};

#endif /* __LUNAIX_SIGNAL_DEFS_H */
