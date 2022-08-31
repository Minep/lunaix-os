#include <lunaix/ds/rwlock.h>
#include <lunaix/spike.h>

void
rwlock_init(rwlock_t* rwlock)
{
    waitq_init(&rwlock->waiting_readers);
    waitq_init(&rwlock->waiting_writers);
    atomic_init(&rwlock->readers, 0);
    atomic_flag_clear(&rwlock->writer);
}

void
rwlock_begin_read(rwlock_t* rwlock)
{
    while (atomic_flag_test_and_set(&rwlock->writer)) {
        pwait(&rwlock->waiting_readers);
    }
    atomic_fetch_add(&rwlock->readers, 1);
    atomic_flag_clear(&rwlock->writer);
    pwake_all(&rwlock->waiting_readers);
}

void
rwlock_end_read(rwlock_t* rwlock)
{
    assert(atomic_load(&rwlock->readers) > 0);
    atomic_fetch_sub(&rwlock->readers, 1);

    if (!atomic_load(&rwlock->readers)) {
        pwake_one(&rwlock->waiting_writers);
    }
}

void
rwlock_begin_write(rwlock_t* rwlock)
{
    // first, acquire writer lock, prevent any incoming readers
    while (atomic_flag_test_and_set(&rwlock->writer)) {
        pwait(&rwlock->waiting_writers);
    }

    // then, wait for reader finish the read.
    while (atomic_load(&rwlock->readers)) {
        pwait(&rwlock->waiting_writers);
    }
}

void
rwlock_end_write(rwlock_t* rwlock)
{
    atomic_flag_clear(&rwlock->writer);
    if (waitq_empty(&rwlock->waiting_writers)) {
        pwake_all(&rwlock->waiting_readers);
    } else {
        pwake_one(&rwlock->waiting_writers);
    }
}