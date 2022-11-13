/**
 * @file timer.c
 * @author Lunaixsky
 * @brief A simple timer implementation based on APIC with adjustable frequency
 * and subscribable "timerlets"
 * @version 0.1
 * @date 2022-03-12
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <arch/x86/interrupts.h>
#include <hal/apic.h>
#include <hal/rtc.h>

#include <lunaix/isrm.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/sched.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <lunaix/timer.h>

#include <hal/acpi/acpi.h>

#define LVT_ENTRY_TIMER(vector, mode) (LVT_DELIVERY_FIXED | mode | vector)

LOG_MODULE("TIMER");

static void
temp_intr_routine_rtc_tick(const isr_param* param);

static void
temp_intr_routine_apic_timer(const isr_param* param);

static void
timer_update(const isr_param* param);

static volatile struct lx_timer_context* timer_ctx = NULL;

// Don't optimize them! Took me an half hour to figure that out...

static volatile u32_t rtc_counter = 0;
static volatile uint8_t apic_timer_done = 0;

static volatile u32_t sched_ticks = 0;
static volatile u32_t sched_ticks_counter = 0;

static struct cake_pile* timer_pile;

#define APIC_CALIBRATION_CONST 0x100000

void
timer_init_context()
{
    timer_pile = cake_new_pile("timer", sizeof(struct lx_timer), 1, 0);
    timer_ctx =
      (struct lx_timer_context*)valloc(sizeof(struct lx_timer_context));

    assert_msg(timer_ctx, "Fail to initialize timer contex");

    timer_ctx->active_timers = (struct lx_timer*)cake_grab(timer_pile);
    llist_init_head(&timer_ctx->active_timers->link);
}

void
timer_init(u32_t frequency)
{
    timer_init_context();

    cpu_disable_interrupt();

    // Setup APIC timer

    // Remap the IRQ 8 (rtc timer's vector) to RTC_TIMER_IV in ioapic
    //       (Remarks IRQ 8 is pin INTIN8)
    //       See IBM PC/AT Technical Reference 1-10 for old RTC IRQ
    //       See Intel's Multiprocessor Specification for IRQ - IOAPIC INTIN
    //       mapping config.

    // grab ourselves these irq numbers
    u32_t iv_rtc = isrm_bindirq(PC_AT_IRQ_RTC, temp_intr_routine_rtc_tick);
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
        kprintf(KWARN "Frequency too low. Millisecond timer might be dodgy.");
    }
#endif

    timer_ctx->base_frequency = 0;
    rtc_counter = 0;
    apic_timer_done = 0;

    rtc_enable_timer();                                     // start RTC timer
    apic_write_reg(APIC_TIMER_ICR, APIC_CALIBRATION_CONST); // start APIC timer

    // enable interrupt, just for our RTC start ticking!
    cpu_enable_interrupt();

    wait_until(apic_timer_done);

    assert_msg(timer_ctx->base_frequency, "Fail to initialize timer (NOFREQ)");

    kprintf(
      KINFO "hw: %u Hz; os: %u Hz\n", timer_ctx->base_frequency, frequency);

    timer_ctx->running_frequency = frequency;
    timer_ctx->tphz = timer_ctx->base_frequency / frequency;

    // cleanup
    isrm_ivfree(iv_timer);
    isrm_ivfree(iv_rtc);

    apic_write_reg(
      APIC_TIMER_LVT,
      LVT_ENTRY_TIMER(isrm_ivexalloc(timer_update), LVT_TIMER_PERIODIC));

    apic_write_reg(APIC_TIMER_ICR, timer_ctx->tphz);

    sched_ticks = timer_ctx->running_frequency / 1000 * SCHED_TIME_SLICE;
    sched_ticks_counter = 0;
}

struct lx_timer*
timer_run_second(u32_t second,
                 void (*callback)(void*),
                 void* payload,
                 uint8_t flags)
{
    return timer_run(
      second * timer_ctx->running_frequency, callback, payload, flags);
}

struct lx_timer*
timer_run_ms(u32_t millisecond,
             void (*callback)(void*),
             void* payload,
             uint8_t flags)
{
    return timer_run(timer_ctx->running_frequency / 1000 * millisecond,
                     callback,
                     payload,
                     flags);
}

struct lx_timer*
timer_run(ticks_t ticks, void (*callback)(void*), void* payload, uint8_t flags)
{
    struct lx_timer* timer = (struct lx_timer*)cake_grab(timer_pile);

    if (!timer)
        return NULL;

    timer->callback = callback;
    timer->counter = ticks;
    timer->deadline = ticks;
    timer->payload = payload;
    timer->flags = flags;

    llist_append(&timer_ctx->active_timers->link, &timer->link);

    return timer;
}

static void
timer_update(const isr_param* param)
{
    struct lx_timer *pos, *n;
    struct lx_timer* timer_list_head = timer_ctx->active_timers;

    llist_for_each(pos, n, &timer_list_head->link, link)
    {
        if (--(pos->counter)) {
            continue;
        }

        pos->callback ? pos->callback(pos->payload) : 1;

        if ((pos->flags & TIMER_MODE_PERIODIC)) {
            pos->counter = pos->deadline;
        } else {
            llist_delete(&pos->link);
            cake_release(timer_pile, pos);
        }
    }

    sched_ticks_counter++;

    if (sched_ticks_counter >= sched_ticks) {
        sched_ticks_counter = 0;
        schedule();
    }
}

static void
temp_intr_routine_rtc_tick(const isr_param* param)
{
    rtc_counter++;

    // dummy read on register C so RTC can send anther interrupt
    //  This strange behaviour observed in virtual box & bochs
    (void)rtc_read_reg(RTC_REG_C);
}

static void
temp_intr_routine_apic_timer(const isr_param* param)
{
    timer_ctx->base_frequency =
      APIC_CALIBRATION_CONST / rtc_counter * RTC_TIMER_BASE_FREQUENCY;
    apic_timer_done = 1;

    rtc_disable_timer();
}

struct lx_timer_context*
timer_context()
{
    return timer_ctx;
}