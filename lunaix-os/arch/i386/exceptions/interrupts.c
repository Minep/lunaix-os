#include <sys/interrupts.h>
#include <sys/x86_isa.h>

#include <hal/cpu.h>
#include <hal/intc.h>

#include <lunaix/isrm.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/syslog.h>
#include <lunaix/tty/tty.h>

#include <sys/i386_intr.h>

LOG_MODULE("INTR")

void
intr_handler(isr_param* param)
{
    param->execp->saved_prev_ctx = __current->intr_ctx;
    __current->intr_ctx = param;

    volatile struct exec_param* execp = __current->intr_ctx->execp;

    if (execp->vector <= 255) {
        isr_cb subscriber = isrm_get(execp->vector);
        subscriber(param);
        goto done;
    }

    kprint_panic("INT %u: (%x) [%p: %p] Unknown",
                 execp->vector,
                 execp->err_code,
                 execp->cs,
                 execp->eip);

done:

    intc_notify_eoi(0, execp->vector);

    return;
}