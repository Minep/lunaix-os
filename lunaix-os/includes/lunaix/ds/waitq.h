#ifndef __LUNAIX_CODVAR_H
#define __LUNAIX_CODVAR_H

#include <lunaix/ds/llist.h>
#include <lunaix/sched.h>

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

void
pwait(waitq_t* queue);

void
pwake_one(waitq_t* queue);

void
pwake_all(waitq_t* queue);

#define wait_if(cond)                                                          \
    while ((cond)) {                                                           \
        sched_yieldk();                                                        \
    }

#endif /* __LUNAIX_CODVAR_H */
