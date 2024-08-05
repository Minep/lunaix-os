#include <lunaix/ds/waitq.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/spike.h>
#include <lunaix/kpreempt.h>

static inline void must_inline
__try_wait(bool check_stall) 
{
    unsigned int nstall;
    waitq_t* current_wq = &current_thread->waitqueue;
    if (waitq_empty(current_wq)) {
        return;
    }
    
    block_current_thread();

    if (!check_stall) {
        // if we are not checking stall, we give up voluntarily
        yield_current();
    } else {
        // otherwise, treat it as being preempted by kernel
        preempt_current();
    }

    // In case of SIGINT-forced awaken
    llist_delete(&current_wq->waiters);
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
    if (llist_empty(&queue->waiters)) {
        return;
    }

    waitq_t* wq = list_entry(queue->waiters.next, waitq_t, waiters);
    struct thread* thread = container_of(wq, struct thread, waitqueue);

    assert(thread->state == PS_BLOCKED);
    thread->state = PS_READY;
    llist_delete(&wq->waiters);
}

void
pwake_all(waitq_t* queue)
{
    if (llist_empty(&queue->waiters)) {
        return;
    }

    struct thread* thread;
    waitq_t *pos, *n;
    llist_for_each(pos, n, &queue->waiters, waiters)
    {
        thread = container_of(pos, struct thread, waitqueue);

        if (thread->state == PS_BLOCKED) {
            thread->state = PS_READY;
        }
        
        // already awaken or killed by other event, just remove it
        llist_delete(&pos->waiters);
    }
}

void
prepare_to_wait(waitq_t* queue)
{
    assert(current_thread);
    
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
