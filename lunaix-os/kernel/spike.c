#include <klibc/strfmt.h>
#include <lunaix/spike.h>
#include <lunaix/hart_state.h>
#include <lunaix/syslog.h>
#include <lunaix/trace.h>
#include <lunaix/failsafe.h>

LOG_MODULE("spike")

void noret
__assert_fail(const char* expr, const char* file, unsigned int line)
{
    // Don't do another trap, print it right-away, allow
    //  the stack context being preserved
    cpu_disable_interrupt();
    ERROR("assertion fail (%s:%u)\n\t%s", file, line, expr);
    
    failsafe_diagnostic();
}