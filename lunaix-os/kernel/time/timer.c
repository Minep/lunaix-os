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

#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/sched.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <lunaix/timer.h>
#include <lunaix/hart_state.h>

#include <hal/hwtimer.h>

LOG_MODULE("TIMER");

static void
timer_update();

static volatile struct lx_timer_context* timer_ctx = NULL;

static volatile u32_t sched_ticks = 0;
static volatile u32_t sched_ticks_counter = 0;

static struct cake_pile* timer_pile;

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
timer_init()
{
    timer_init_context();

    hwtimer_init(SYS_TIMER_FREQUENCY_HZ, timer_update);

    timer_ctx->base_frequency = hwtimer_base_frequency();

    sched_ticks = (SYS_TIMER_FREQUENCY_HZ * SCHED_TIME_SLICE) / 1000;
    sched_ticks_counter = 0;
}

struct lx_timer*
timer_run_second(u32_t second,
                 void (*callback)(void*),
                 void* payload,
                 u8_t flags)
{
    ticks_t t = hwtimer_to_ticks(second, TIME_SEC);
    return timer_run(t, callback, payload, flags);
}

struct lx_timer*
timer_run_ms(u32_t millisecond,
             void (*callback)(void*),
             void* payload,
             u8_t flags)
{
    ticks_t t = hwtimer_to_ticks(millisecond, TIME_MS);
    return timer_run(t, callback, payload, flags);
}

struct lx_timer*
timer_run(ticks_t ticks, void (*callback)(void*), void* payload, u8_t flags)
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
timer_update()
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
        thread_stats_update_entering(false);
        schedule();
    }
}

struct lx_timer_context*
timer_context()
{
    return (struct lx_timer_context*)timer_ctx;
}