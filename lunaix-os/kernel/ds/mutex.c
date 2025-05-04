#include <lunaix/ds/mutex.h>
#include <lunaix/process.h>
#include <lunaix/kpreempt.h>

#define __do_lock(mutext)               \
    ({                                  \
        atomic_fetch_add(&mutex->lk, 1);\
        mutex->owner = __current->pid;  \
    })

static inline bool must_inline
__mutex_check_owner(mutex_t* mutex)
{
    return mutex->owner == __current->pid;
}

static inline void must_inline
__mutext_lock(mutex_t* mutex)
{
    while (atomic_load(&mutex->lk)) {
        preempt_current();
    }

    __do_lock(mutex);
}

static inline void must_inline
__mutext_unlock(mutex_t* mutex)
{
    if (__mutex_check_owner(mutex))
        atomic_fetch_sub(&mutex->lk, 1);
}

void
mutex_lock(mutex_t* mutex)
{
    __mutext_lock(mutex);
}

bool
mutex_trylock(mutex_t* mutex)
{
    if (atomic_load(&mutex->lk))
        return false;

    __do_lock(mutex);
    return true;
}

void
mutex_unlock(mutex_t* mutex)
{
    __mutext_unlock(mutex);
}

void
mutex_unlock_for(mutex_t* mutex, pid_t pid)
{
    if (mutex->owner != pid || !atomic_load(&mutex->lk)) {
        return;
    }
    atomic_fetch_sub(&mutex->lk, 1);
}

void
mutex_lock_nested(mutex_t* mutex)
{
    if (atomic_load(&mutex->lk) && __mutex_check_owner(mutex)) {
        atomic_fetch_add(&mutex->lk, 1);
        return;
    }

    __mutext_lock(mutex);
}

void
mutex_unlock_nested(mutex_t* mutex)
{
    mutex_unlock_for(mutex, __current->pid);
}