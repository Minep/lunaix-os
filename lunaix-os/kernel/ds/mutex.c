#include <lunaix/ds/mutex.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>

void
mutex_lock(mutex_t* mutex)
{
    if (atomic_load(&mutex->lk) && mutex->owner == __current->pid) {
        atomic_fetch_add(&mutex->lk, 1);
        return;
    }

    while (atomic_load(&mutex->lk)) {
        sched_pass();
    }

    atomic_fetch_add(&mutex->lk, 1);
    mutex->owner = __current->pid;
}

void
mutex_unlock(mutex_t* mutex)
{
    mutex_unlock_for(mutex, __current->pid);
}

void
mutex_unlock_for(mutex_t* mutex, pid_t pid)
{
    if (mutex->owner != pid || !atomic_load(&mutex->lk)) {
        return;
    }
    atomic_fetch_sub(&mutex->lk, 1);
}