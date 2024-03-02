#include <lunaix/failsafe.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <lunaix/trace.h>
#include <lunaix/sched.h>

LOG_MODULE("NMM")

void
do_failsafe_unrecoverable(ptr_t frame_link, ptr_t stack_link)
{
    ERROR("diagnositic mode");

    ERROR("check: init stack: %s", 
            check_bootstack_sanity() ? "ok" : "smashing");

    // TODO ...check other invariants

    ERROR("non recoverable: Nightmare Moon arrival.");

    trace_printstack();

    spin();
}