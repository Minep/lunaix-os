#ifndef __LUNAIX_RWLOCK_H
#define __LUNAIX_RWLOCK_H

#include "mutex.h"
#include "waitq.h"
#include <stdatomic.h>

typedef struct rwlock_s
{
    atomic_uint readers;
    atomic_flag writer;
    waitq_t waiting_readers;
    waitq_t waiting_writers;
} rwlock_t;

void
rwlock_begin_read(rwlock_t* rwlock);

void
rwlock_end_read(rwlock_t* rwlock);

void
rwlock_begin_write(rwlock_t* rwlock);

void
rwlock_end_write(rwlock_t* rwlock);

#endif /* __LUNAIX_RWLOCK_H */
