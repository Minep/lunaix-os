#ifndef __LUNAIX_KPREEMPT_H
#define __LUNAIX_KPREEMPT_H

#include <asm/abi.h>
#include <asm/cpu.h>
#include <lunaix/process.h>

#define not_preemptable(body) \
    do {                      \
	no_preemption();      \
	body;                 \
	set_preemption();     \
    } while(0)

static inline void
set_preemption() 
{
    cpu_enable_interrupt();
}

static inline void
no_preemption() 
{
    cpu_disable_interrupt();
}

static inline void
__schedule_away()
{
    current_thread->stats.last_reentry = clock_systime();
    
    cpu_trap_sched();
    set_preemption();
}

/**
 * @brief preempt the current thread, and yield the remaining
 *        time slice to other threads.
 * 
 *        The current thread is marked as if it is being
 *        preempted involuntarily by kernel.
 * 
 */
static inline void
preempt_current()
{
    no_preemption();
    thread_stats_update_kpreempt();
    __schedule_away();
}

/**
 * @brief yield the remaining time slice to other threads.
 * 
 *        The current thread is marked as if it is being
 *        preempted voluntarily by itself.
 * 
 */
static inline void
yield_current()
{
    no_preemption();
    __schedule_away();
}

bool
preempt_check_stalled(struct thread* th);

#endif /* __LUNAIX_KPREEMPT_H */
