#include <lunaix/spike.h>
#include <hal/hwtimer.h>

struct hwtimer*
select_platform_timer()
{
    fail("unimplemented");
}