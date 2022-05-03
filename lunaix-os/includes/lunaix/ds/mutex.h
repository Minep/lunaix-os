#ifndef __LUNAIX_MUTEX_H
#define __LUNAIX_MUTEX_H

#include "semaphore.h"

// TODO: implement mutex lock

typedef struct sem_t mutex_t;

static inline void mutex_init(mutex_t *mutex) {
    sem_init(mutex, 1);
}

static inline unsigned int mutex_on_hold(mutex_t *mutex) {
    return !atomic_load(&mutex->counter);
}

static inline void mutex_lock(mutex_t *mutex) {
    sem_wait(mutex);
}

static inline void mutex_unlock(mutex_t *mutex) {
    sem_post(mutex);
}

#endif /* __LUNAIX_MUTEX_H */
