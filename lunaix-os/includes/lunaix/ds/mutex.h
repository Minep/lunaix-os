#ifndef __LUNAIX_MUTEX_H
#define __LUNAIX_MUTEX_H

#include "semaphore.h"
#include <lunaix/types.h>

typedef struct mutex_s
{
    struct sem_t sem;
    pid_t owner;
} mutex_t;

static inline void
mutex_init(mutex_t* mutex)
{
    sem_init(&mutex->sem, 1);
}

static inline int
mutex_on_hold(mutex_t* mutex)
{
    return !atomic_load(&mutex->sem.counter);
}

void
mutex_lock(mutex_t* mutex);

void
mutex_unlock(mutex_t* mutex);

void
mutex_unlock_for(mutex_t* mutex, pid_t pid);

#endif /* __LUNAIX_MUTEX_H */
