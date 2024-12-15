#include "apic_timer.h"
#include <hal/hwtimer.h>
#include <hal/irq.h>

#include <lunaix/clock.h>
#include <lunaix/compiler.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

#include "asm/soc/apic.h"

LOG_MODULE("timer(apic)")

#define LVT_ENTRY_TIMER(vector, mode) (LVT_DELIVERY_FIXED | mode | vector)
#define APIC_BASETICKS 0x100000

static void
apic_timer_count_stop(irq_t irq, const struct hart_state* state)
{
    struct hwtimer_pot* pot;

    pot = irq_payload(irq, struct hwtimer_pot);
    pot->systick_raw = (ticks_t)-1;
}

static void
apic_timer_tick_isr(irq_t irq, const struct hart_state* state)
{
    struct hwtimer_pot* pot;

    pot = irq_payload(irq, struct hwtimer_pot);
    pot->systick_raw++;

    if (likely(__ptr(pot->callback))) {
        pot->callback();
    }
}

static void
__apic_timer_calibrate(struct hwtimer_pot* pot, u32_t hertz)
{
    irq_t irq;
    ticks_t base_freq = 0;

    cpu_disable_interrupt();

    irq = irq_declare_direct(apic_timer_count_stop);
    irq_set_payload(irq, pot);
    irq_assign(irq_get_default_domain(), irq, NULL);

    // Setup a one-shot timer, we will use this to measure the bus speed. So we
    // can then calibrate apic timer to work at *nearly* accurate hz
    apic_write_reg(APIC_TIMER_LVT,
                   LVT_ENTRY_TIMER(irq->vector, LVT_TIMER_ONESHOT));

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

    sysrtc->ops->set_proactive(sysrtc, true);
    apic_write_reg(APIC_TIMER_ICR, APIC_BASETICKS); // start APIC timer

    pot->systick_raw = 0;
    cpu_enable_interrupt();

    wait_until(!(pot->systick_raw + 1));

    cpu_disable_interrupt();

    sysrtc->ops->set_proactive(sysrtc, false);
    
    base_freq = sysrtc->live;
    base_freq = APIC_BASETICKS / base_freq * sysrtc->base_freq;
    pot->base_freq = base_freq;
    pot->systick_raw = 0;

    assert_msg(base_freq, "Fail to initialize timer (NOFREQ)");
    INFO("hw: %u Hz; os: %u Hz", base_freq, hertz);

    irq_set_servant(irq, apic_timer_tick_isr);

    apic_write_reg(APIC_TIMER_ICR, base_freq / hertz);
    apic_write_reg(
        APIC_TIMER_LVT,
        LVT_ENTRY_TIMER(irq->vector, LVT_TIMER_PERIODIC));
}

static struct hwtimer_pot_ops potops = {
    .calibrate = __apic_timer_calibrate,
};

static int
__apic_timer_load(struct device_def* def)
{
    struct device* timer;

    timer = device_allocsys(NULL, NULL);

    hwtimer_attach_potens(timer, HWTIMER_MAX_PRECEDENCE, &potops);
    register_device_var(timer, &def->class, "apic-timer");
    return 0;
}


static struct device_def x86_apic_timer = 
{
    def_device_class(INTEL, TIME, TIMER_APIC),
    def_device_name("Intel APIC Timer"),
    
    def_on_load(__apic_timer_load)
};
EXPORT_DEVICE(apic_timer, &x86_apic_timer, load_sysconf);
