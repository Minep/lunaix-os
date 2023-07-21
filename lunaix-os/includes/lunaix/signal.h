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
#define _SIG_BLOCK SIG_BLOCK
#define _SIG_UNBLOCK SIG_UNBLOCK
#define _SIG_SETMASK SIG_SETMASK

#define sigset(num) (1 << (num))
#define sigset_add(set, num) (set = set | sigset(num))
#define sigset_test(set, num) (set & sigset(num))
#define sigset_clear(set, num) ((set) = (set) & ~sigset(num))
#define sigset_union(set, set2) ((set) = (set) | (set2))
#define sigset_intersect(set, set2) ((set) = (set) & (set2))

int
signal_send(pid_t pid, int signum);

#endif /* __LUNAIX_SIGNAL_H */
