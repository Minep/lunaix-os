#include <arch/x86/interrupts.h>
#include <arch/x86/tss.h>

#include <hal/apic.h>
#include <hal/cpu.h>

#include <lunaix/isrm.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/syslog.h>
#include <lunaix/tty/tty.h>

LOG_MODULE("INTR")

extern x86_page_table* __kernel_ptd;

void
intr_handler(isr_param* param)
{
    __current->intr_ctx = *param;

    isr_param* lparam = &__current->intr_ctx;

    if (lparam->vector <= 255) {
        isr_cb subscriber = isrm_get(lparam->vector);
        subscriber(param);
        goto done;
    }

    kprint_panic("INT %u: (%x) [%p: %p] Unknown",
                 lparam->vector,
                 lparam->err_code,
                 lparam->cs,
                 lparam->eip);

done:
    // for all external interrupts except the spurious interrupt
    //  this is required by Intel Manual Vol.3A, section 10.8.1 & 10.8.5
    if (lparam->vector >= IV_EX && lparam->vector != APIC_SPIV_IV) {
        apic_done_servicing();
    }

    return;
}