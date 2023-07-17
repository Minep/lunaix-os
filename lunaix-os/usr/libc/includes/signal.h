#ifndef __LUNAIX_SYS_SIGNAL_H
#define __LUNAIX_SYS_SIGNAL_H

#include <lunaix/signal_defs.h>
#include <lunaix/types.h>

extern int
signal(int signum, sighandler_t handler);

extern int
sigpending(sigset_t* set);

extern int
sigsuspend(const sigset_t* mask);

extern int
sigprocmask(int how, const sigset_t* set, sigset_t* oldset);

#endif /* __LUNAIX_SIGNAL_H */
