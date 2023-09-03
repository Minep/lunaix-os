#ifndef __LUNAIX_MUTEX_H
#define __LUNAIX_MUTEX_H

#include <lunaix/types.h>
#include <stdatomic.h>

typedef struct mutex_s
{
    atomic_ulong lk;
    pid_t owner;
} mutex_t;

static inline void
mutex_init(mutex_t* mutex)
{
    mutex->lk = ATOMIC_VAR_INIT(0);
}

static inline int
mutex_on_hold(mutex_t* mutex)
{
    return atomic_load(&mutex->lk);
}

void
mutex_lock(mutex_t* mutex);

void
mutex_unlock(mutex_t* mutex);

void
mutex_unlock_for(mutex_t* mutex, pid_t pid);

#endif /* __LUNAIX_MUTEX_H */
