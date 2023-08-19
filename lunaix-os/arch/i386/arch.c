#include <hal/apic_timer.h>
#include <hal/rtc/mc146818a.h>

#include <hal/hwrtc.h>
#include <hal/hwtimer.h>

#include <lunaix/isrm.h>
#include <lunaix/spike.h>

#include <sys/i386_intr.h>
#include <sys/interrupts.h>

void
exception_init()
{
    exception_install_handler();
    isrm_init();
    intr_routine_init();
}

extern void
syscall_hndlr(const isr_param* param);

void
arch_preinit()
{
    exception_init();

    isrm_bindiv(LUNAIX_SYS_CALL, syscall_hndlr);
}

struct hwtimer_context*
hwtimer_choose()
{
    struct hwtimer_context* timer;

    timer = apic_hwtimer_context();
    if (timer->supported(timer)) {
        return timer;
    }

    // TODO select alternatives...

    panick("no timer to use.");
}

struct hwrtc*
hwrtc_choose()
{
    struct hwrtc* rtc = mc146818a_rtc_context();

    return rtc;
}