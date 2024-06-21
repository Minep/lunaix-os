#include "apic_timer.h"
#include <hal/hwtimer.h>

#include <lunaix/clock.h>
#include <lunaix/compiler.h>
#include <lunaix/generic/isrm.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

#include "sys/apic.h"

LOG_MODULE("APIC_TIMER")

#define LVT_ENTRY_TIMER(vector, mode) (LVT_DELIVERY_FIXED | mode | vector)
#define APIC_BASETICKS 0x100000

// Don't optimize them! Took me an half hour to figure that out...

static volatile u8_t apic_timer_done = 0;
static volatile ticks_t base_freq = 0;
static volatile ticks_t systicks = 0;

static timer_tick_cb tick_cb = NULL;

static void
temp_intr_routine_apic_timer(const isr_param* param)
{
    apic_timer_done = 1;
}

static void
apic_timer_tick_isr(const isr_param* param)
{
    systicks++;

    if (likely((ptr_t)tick_cb)) {
        tick_cb();
    }
}

static int
apic_timer_check(struct hwtimer* hwt)
{
    // TODO check whether apic timer is supported
    return 1;
}

static ticks_t
apic_get_systicks()
{
    return systicks;
}

static ticks_t
apic_get_base_freq()
{
    return base_freq;
}

void
apic_timer_init(struct hwtimer* timer, u32_t hertz, timer_tick_cb timer_cb)
{
    ticks_t frequency = hertz;
    tick_cb = timer_cb;

    cpu_disable_interrupt();

    // Setup APIC timer

    // Remap the IRQ 8 (rtc timer's vector) to RTC_TIMER_IV in ioapic
    //       (Remarks IRQ 8 is pin INTIN8)
    //       See IBM PC/AT Technical Reference 1-10 for old RTC IRQ
    //       See Intel's Multiprocessor Specification for IRQ - IOAPIC INTIN
    //       mapping config.

    // grab ourselves these irq numbers
    u32_t iv_timer = isrm_ivexalloc(temp_intr_routine_apic_timer);

    // Setup a one-shot timer, we will use this to measure the bus speed. So we
    // can then calibrate apic timer to work at *nearly* accurate hz
    apic_write_reg(APIC_TIMER_LVT,
                   LVT_ENTRY_TIMER(iv_timer, LVT_TIMER_ONESHOT));

    // Set divider to 64
    apic_write_reg(APIC_TIMER_DCR, APIC_TIMER_DIV64);

    /*
        Timer calibration process - measure the APIC timer base frequency

         step 1: setup a temporary isr for RTC timer which trigger at each tick
                 (1024Hz)
         step 2: setup a temporary isr for #APIC_TIMER_IV
         step 3: setup the divider, APIC_TIMER_DCR
         step 4: Startup RTC timer
         step 5: Write a large value, v, to APIC_TIMER_ICR to start APIC timer
       (this must be followed immediately after step 4) step 6: issue a write to
       EOI and clean up.

        When the APIC ICR counting down to 0 #APIC_TIMER_IV triggered, save the
       rtc timer's counter, k, and disable RTC timer immediately (although the
       RTC interrupts should be blocked by local APIC as we are currently busy
       on handling #APIC_TIMER_IV)

        So the apic timer frequency F_apic in Hz can be calculate as
            v / F_apic = k / 1024
            =>  F_apic = v / k * 1024

    */

#ifdef __LUNAIXOS_DEBUG__
    if (frequency < 1000) {
        WARN("Frequency too low. Millisecond timer might be dodgy.");
    }
#endif

    apic_timer_done = 0;

    sysrtc->cls_mask(sysrtc);
    apic_write_reg(APIC_TIMER_ICR, APIC_BASETICKS); // start APIC timer

    // enable interrupt, just for our RTC start ticking!
    cpu_enable_interrupt();

    wait_until(apic_timer_done);

    cpu_disable_interrupt();

    sysrtc->set_mask(sysrtc);

    base_freq = sysrtc->get_counts(sysrtc);
    base_freq = APIC_BASETICKS / base_freq * sysrtc->base_freq;

    assert_msg(base_freq, "Fail to initialize timer (NOFREQ)");

    kprintf("hw: %u Hz; os: %u Hz", base_freq, frequency);

    // cleanup
    isrm_ivfree(iv_timer);

    ticks_t tphz = base_freq / frequency;
    timer->base_freq = base_freq;
    apic_write_reg(APIC_TIMER_ICR, tphz);

    apic_write_reg(
      APIC_TIMER_LVT,
      LVT_ENTRY_TIMER(isrm_ivexalloc(apic_timer_tick_isr), LVT_TIMER_PERIODIC));
}

struct hwtimer*
apic_hwtimer_context()
{
    static struct hwtimer apic_hwt = {
        .name = "apic_timer",
        .class = DEVCLASSV(DEVIF_SOC, DEVFN_TIME, DEV_TIMER, DEV_TIMER_APIC),
        .init = apic_timer_init,
        .supported = apic_timer_check,
        .systicks = apic_get_systicks
    };

    return &apic_hwt;
}
