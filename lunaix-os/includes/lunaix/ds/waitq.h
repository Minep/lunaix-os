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

void
pwait(waitq_t* queue);

void
pwake_one(waitq_t* queue);

void
pwake_all(waitq_t* queue);

#endif /* __LUNAIX_CODVAR_H */
