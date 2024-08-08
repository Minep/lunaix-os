#ifndef __LUNAIX_KPREEMPT_H
#define __LUNAIX_KPREEMPT_H

#include <sys/abi.h>
#include <sys/cpu.h>
#include <lunaix/process.h>

#define _preemptible \
        __attribute__((section(".kf.preempt"))) no_inline

#define ensure_preempt_caller()                                 \
    do {                                                        \
        extern int __kf_preempt_start[];                        \
        extern int __kf_preempt_end[];                          \
        ptr_t _retaddr = abi_get_retaddr();                     \
        assert_msg((ptr_t)__kf_preempt_start <= _retaddr        \
                    && _retaddr < (ptr_t)__kf_preempt_end,      \
                   "caller must be kernel preemptible");        \
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
