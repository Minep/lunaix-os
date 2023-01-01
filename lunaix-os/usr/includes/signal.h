#ifndef __LUNAIX_SYS_SIGNAL_H
#define __LUNAIX_SYS_SIGNAL_H

#include <signal_defs.h>
#include <sys/types.h>

int
signal(int signum, sighandler_t handler);

int
sigpending(sigset_t* set);

int
sigsuspend(const sigset_t* mask);

int
sigprocmask(int how, const sigset_t* set, sigset_t* oldset);

#endif /* __LUNAIX_SIGNAL_H */
