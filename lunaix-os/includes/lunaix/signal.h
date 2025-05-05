#ifndef __LUNAIX_SIGNAL_H
#define __LUNAIX_SIGNAL_H

#include <lunaix/types.h>
#include <usr/lunaix/signal.h>

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

struct sigact
{
    sigset_t sa_mask;
    void* sa_actor;
    void* sa_handler;
    pid_t sender;
};

struct sigregistry {
    struct sigact* signals[_SIG_NUM];
};

struct sigctx
{
    sigset_t sig_pending;
    sigset_t sig_mask;
    signum_t sig_active;
    signum_t sig_order[_SIG_NUM];
};

int
signal_send(pid_t pid, signum_t signum);

void
signal_dup_context(struct sigctx* dest_ctx);

void
signal_dup_registry(struct sigregistry* dest_reg);

void
signal_reset_context(struct sigctx* sigctx);

void
signal_reset_registry(struct sigregistry* sigreg);

void
signal_free_registry(struct sigregistry* sigreg);

#endif /* __LUNAIX_SIGNAL_H */
