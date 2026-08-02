#include <lunaix/failsafe.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <lunaix/trace.h>
#include <lunaix/sched.h>

LOG_MODULE("NMM")

void
do_failsafe_unrecoverable(ptr_t frame_link, ptr_t stack_link)
{
    FATAL("------- [cut here] ------- \n");
    FATAL("diagnositic mode");
    FATAL("check: init stack: %s", 
            check_bootstack_sanity() ? "ok" : "smashing");

    // TODO ...check other invariants
    if (current_thread && current_thread->hstate)
    {
        struct hart_state* hstate = current_thread->hstate;
    
        trace_print_transition_full(hstate);
        FATAL("++++++");

        trace_dump_state(hstate);
        FATAL("++++++");
    }

    trace_printstack();
    FATAL("++++++");
    
    FATAL("non recoverable: Nightmare Moon arrival.");
    spin();
}
