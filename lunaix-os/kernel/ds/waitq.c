#include <lunaix/ds/waitq.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/spike.h>
#include <lunaix/kpreempt.h>

/**
 * The pwait/wait_queue
 * ----------
 *
 * the pwait/wait_queue is a universal solution to suspend 
 * current executing thread until some user-specific event
 * is occured or the wait is canceled.
 *
 * User can define such event by declaring a wait queue and 
 * the event can be made aware to waiting thread by manually
 * calling pwake(), thus detaching the thread (or waiter) 
 * from the said wait queue.
 *
 *
 * How it Works
 * -----------
 * Upon calling pwait(), it move the thread into PS_ONWAIT 
 * state and yield the time slot (depending on checking 
 * stalling or not, it might use different method to do
 * the yield, each method will contribute differently to
 * the thread stats).
 *
 * The scheduler will check the wait queue belongings of
 * the waiting thread (PS_ONWAIT) and resume the thread 
 * automatically if the thread is detached from the queue.
 *
 */ 

static inline void must_inline
__try_wait(bool check_stall) 
{
    unsigned int nstall;

    // FIXME [2026-QUALIFIER] volatile
    waitq_t* current_wq = &current_thread->waitqueue;

    if (waitq_empty(current_wq)) {
        return;
    }

    wait_current_thread();

    if (!check_stall) {
        // if we are not checking stall, we give up voluntarily
        yield_current();
    } else {
        // otherwise, treat it as being preempted by kernel
        preempt_current();
    }
}

static inline void must_inline
__pwait(waitq_t* queue, bool check_stall)
{
    // prevent race condition.
    no_preemption();

    prepare_to_wait(queue);
    __try_wait(check_stall);

    set_preemption();
}

void
pwait(waitq_t* queue)
{
    __pwait(queue, false);
}

void
pwait_check_stall(waitq_t* queue)
{
    __pwait(queue, true);
}

void
pwake_one(waitq_t* queue)
{
    if (waitq_empty(queue)) {
        return;
    }

    waitq_t* wq = list_entry(queue->waiters.next, waitq_t, waiters);
    llist_delete(&wq->waiters);
}

void
pwake_all(waitq_t* queue)
{
    if (waitq_empty(queue)) {
        return;
    }

    struct thread* thread;
    waitq_t *pos, *n;
    llist_for_each(pos, n, &queue->waiters, waiters)
    {
        // already awaken or killed by other event, just remove it
        llist_delete(&pos->waiters);
    }
}

void
prepare_to_wait(waitq_t* queue)
{
    assert(current_thread);
    
    // FIXME [2026-QUALIFIER] volatile
    waitq_t* current_wq = &current_thread->waitqueue;
    assert(llist_empty(&current_wq->waiters));

    llist_append(&queue->waiters, &current_wq->waiters);
}

void
try_wait()
{
    __try_wait(false);
}

void
try_wait_check_stall()
{
    __try_wait(true);
}
