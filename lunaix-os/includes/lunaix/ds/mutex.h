#ifndef __LUNAIX_MUTEX_H
#define __LUNAIX_MUTEX_H

#include "asm/atom_ops.h"
#include <lunaix/atomic.h>
#include <lunaix/types.h>

typedef struct mutex_s
{
    int lk;
    pid_t owner;
} mutex_t;

static inline void
mutex_init(mutex_t* mutex)
{
    mutex->lk = 0;
}

static inline int
mutex_on_hold(mutex_t* mutex)
{
    return atom_ldi(&mutex->lk);
}

void
mutex_lock(mutex_t* mutex);

void
mutex_unlock(mutex_t* mutex);

void
mutex_lock_nested(mutex_t* mutex);

void
mutex_unlock_nested(mutex_t* mutex);

void
mutex_unlock_for(mutex_t* mutex, pid_t pid);


#endif /* __LUNAIX_MUTEX_H */
