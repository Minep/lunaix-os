#ifndef __LUNAIX_CODVAR_H
#define __LUNAIX_CODVAR_H

#include <lunaix/ds/llist.h>

typedef struct waitq
{
    struct llist_header waiters;
} waitq_t;

static inline void
waitq_init(waitq_t* waitq)
{
    llist_init_head(&waitq->waiters);
}

static inline int
waitq_empty(waitq_t* waitq)
{
    return llist_empty(&waitq->waiters);
}

static inline void
waitq_cancel_wait(waitq_t* waitq)
{
    llist_delete(&waitq->waiters);
}

void
prepare_to_wait(waitq_t* waitq);

void
try_wait();

void
try_wait_check_stall();

void
pwait(waitq_t* queue);

void
pwait_check_stall(waitq_t* queue);

void
pwake_one(waitq_t* queue);

void
pwake_all(waitq_t* queue);

#endif /* __LUNAIX_CODVAR_H */
