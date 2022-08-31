#include <lunaix/ds/mutex.h>
#include <lunaix/process.h>

void
mutex_lock(mutex_t* mutex)
{
    sem_wait(&mutex->sem);
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
    if (mutex->owner != pid) {
        return;
    }
    sem_post(&mutex->sem);
}