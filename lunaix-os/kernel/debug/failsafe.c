#include <lunaix/failsafe.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <lunaix/trace.h>
#include <lunaix/sched.h>

LOG_MODULE("NMM")

void
do_failsafe_unrecoverable(ptr_t frame_link, ptr_t stack_link)
{
    ERROR("------- [cut here] ------- \n");
    ERROR("diagnositic mode");
    ERROR("check: init stack: %s", 
            check_bootstack_sanity() ? "ok" : "smashing");

    // TODO ...check other invariants
    if (current_thread && current_thread->hstate)
    {
        struct hart_state* hstate = current_thread->hstate;
    
        trace_print_transition_full(hstate);
        ERROR("++++++");

        trace_dump_state(hstate);
        ERROR("++++++");
    }

    trace_printstack();
    ERROR("++++++");
    
    ERROR("non recoverable: Nightmare Moon arrival.");
    spin();
}