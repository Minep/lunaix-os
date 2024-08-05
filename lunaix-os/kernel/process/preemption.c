#include <lunaix/kpreempt.h>
#include <lunaix/process.h>
#include <lunaix/switch.h>
#include <lunaix/syslog.h>
#include <lunaix/trace.h>

LOG_MODULE("preempt");

#ifdef CONFIG_CHECK_STALL
bool
preempt_check_stalled(struct thread* th)
{
    // we can't access the hart state here
    //  as th might be in other address space

    if (thread_flags_test(th, TH_STALLED))
    {
        // alrady stalled, no need to concern
        return false;
    }

    struct thread_stats* stats;
    stats   = &current_thread->stats;

    if (!stats->kpreempt_count) {
        return false;
    }

    if (stats->at_user) {
        return false;
    }

#if defined(CONFIG_STALL_MAX_PREEMPTS) && CONFIG_STALL_MAX_PREEMPTS
    if (stats->kpreempt_count > CONFIG_STALL_MAX_PREEMPTS) {
        return true;
    }
#endif

    ticks_t total_elapsed;

    total_elapsed = thread_stats_kernel_elapse(th);
    return total_elapsed > ticks_seconds(CONFIG_STALL_TIMEOUT);
}

#else 
bool
preempt_check_stalled(struct thread* th)
{
    return false;
}

#endif

void
preempt_handle_stalled(struct signpost_result* result)
{
    if (!thread_flags_test(current_thread, TH_STALLED))
    {
        continue_switch(result);
        return;
    }

    kprintf("+++++ ");
    kprintf(" stalling detected (pid: %d, tid: %d)", 
                __current->pid, current_thread->tid);
    kprintf(" (preempted: %d, elapsed: %dms)", 
                current_thread->stats.kpreempt_count, 
                thread_stats_kernel_elapse(current_thread));
    kprintf("   are you keeping Luna too busy?");
    kprintf("+++++ ");

    kprintf("last known state:");
    trace_dump_state(current_thread->hstate);

    kprintf("trace from the point of stalling:");
    trace_printstack_isr(current_thread->hstate);

    kprintf("thread is blocked");

    block_current_thread();
    giveup_switch(result);
}