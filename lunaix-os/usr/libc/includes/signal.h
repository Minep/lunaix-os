#ifndef __LUNALIBC_SYS_SIGNAL_H
#define __LUNALIBC_SYS_SIGNAL_H

#include <lunaix/signal.h>
#include <sys/types.h>

extern sighandler_t
signal(int signum, sighandler_t handler);

extern int
kill(pid_t pid, int signum);

extern int
raise(int signum);

extern int
sigaction(int signum, struct sigaction* action);

extern int
sigpending(sigset_t* set);

extern int
sigsuspend(const sigset_t* mask);

extern int
sigprocmask(int how, const sigset_t* set, sigset_t* oldset);

int 
pthread_sigmask(int how, const sigset_t *restrict set,
                    sigset_t *restrict oset);

#endif /* __LUNALIBC_SIGNAL_H */
