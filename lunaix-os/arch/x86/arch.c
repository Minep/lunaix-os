#include <hal/hwtimer.h>

#include <lunaix/generic/isrm.h>
#include <lunaix/spike.h>
#include <lunaix/process.h>

#include "sys/int_handler.h"
#include "sys/x86_isa.h"
#include "sys/hart.h"

#include "hal/apic_timer.h"

void
exception_init()
{
    exception_install_handler();
    isrm_init();
    intr_routine_init();
}

extern void
syscall_hndlr(const struct hart_state* hstate);

void
arch_preinit()
{
    exception_init();

    isrm_bindiv(LUNAIX_SYS_CALL, syscall_hndlr);
}

struct hwtimer*
select_platform_timer()
{
    struct hwtimer* timer;

    timer = apic_hwtimer_context();
    if (timer->supported(timer)) {
        return timer;
    }

    // TODO select alternatives...

    panick("no timer to use.");
}

void
update_tss()
{
    extern struct x86_tss _tss;
#ifdef CONFIG_ARCH_X86_64
    _tss.rsps[0] = (ptr_t)current_thread->hstate;
#else
    _tss.esp0 = (u32_t)current_thread->hstate;
#endif
}