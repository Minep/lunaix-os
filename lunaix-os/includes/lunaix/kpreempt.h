#ifndef __LUNAIX_KPREEMPT_H
#define __LUNAIX_KPREEMPT_H

#include <asm/abi.h>
#include <asm/cpu.h>
#include <lunaix/process.h>

typedef struct arch_irqflag irqflag_t;

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
no_preemption_saved(irqflag_t* flag)
{
    cpu_save_irqflag(flag);
    cpu_disable_interrupt();
}

static inline void
set_preemption_saved(irqflag_t* flag)
{
    cpu_save_irqflag(flag);
    cpu_enable_interrupt();
}

static inline void
preempt_restore(irqflag_t* flag)
{
    cpu_restore_irqflag(flag);
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
