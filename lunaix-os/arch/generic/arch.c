#include <hal/hwtimer.h>
#include <lunaix/spike.h>

_default struct hwtimer*
select_platform_timer()
{
    fail("not implemented");
}