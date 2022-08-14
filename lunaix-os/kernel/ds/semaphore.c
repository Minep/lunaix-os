#include <lunaix/ds/semaphore.h>
#include <lunaix/sched.h>

void
sem_init(struct sem_t* sem, unsigned int initial)
{
    sem->counter = ATOMIC_VAR_INIT(initial);
}

void
sem_wait(struct sem_t* sem)
{
    while (!atomic_load(&sem->counter)) {
        // FIXME: better thing like wait queue
        sched_yieldk();
    }
    atomic_fetch_sub(&sem->counter, 1);
}

void
sem_post(struct sem_t* sem)
{
    atomic_fetch_add(&sem->counter, 1);
    // TODO: wake up a thread
}