#include <arch/x86/interrupts.h>
#include <hal/apic.h>
#include <hal/cpu.h>
#include <lunaix/syslog.h>
#include <lunaix/tty/tty.h>

int_subscriber subscribers[256];

static int_subscriber fallback = (int_subscriber) 0;

void
intr_subscribe(const uint8_t vector, int_subscriber subscriber) {
    subscribers[vector] = subscriber;
}

void
intr_unsubscribe(const uint8_t vector, int_subscriber subscriber) {
    if (subscribers[vector] == subscriber) {
        subscribers[vector] = (int_subscriber) 0;
    }
}

void
intr_set_fallback_handler(int_subscriber subscribers) {
    fallback = subscribers;
}

void
intr_handler(isr_param* param)
{
    if (param->vector <= 255) {
        int_subscriber subscriber = subscribers[param->vector];
        if (subscriber) {
            subscriber(param);
            goto done;
        }
    }

    if (fallback) {
        fallback(param);
        goto done;
    }
    
    kprint_panic("INT %u: (%x) [%p: %p] Unknown",
            param->vector,
            param->err_code,
            param->cs,
            param->eip);

done:
    // for all external interrupts except the spurious interrupt
    //  this is required by Intel Manual Vol.3A, section 10.8.1 & 10.8.5
    if (param->vector >= EX_INTERRUPT_BEGIN && param->vector != APIC_SPIV_IV) {
        apic_done_servicing();
    }
    return;
}