#ifndef __LUNAIX_TIMER_H
#define __LUNAIX_TIMER_H

#include <lunaix/ds/llist.h>
#include <lunaix/time.h>
#include <lunaix/hart_state.h>

#define SYS_TIMER_FREQUENCY_HZ 1000

#define TIMER_MODE_PERIODIC 0x1

struct lx_timer_context
{
    struct lx_timer* active_timers;
    /**
     * @brief timer hardware base frequency (ticks per seconds)
     *
     */
    ticks_t base_frequency;
    /**
     * @brief Desired system running frequency
     *
     */
    ticks_t running_frequency;
    /**
     * @brief Ticks per hertz
     *
     */
    ticks_t tphz;
};

struct timer_init_param
{
    struct lx_timer_context* context;
    void* timer_update_isr;
};

struct lx_timer
{
    struct llist_header link;
    ticks_t deadline;
    ticks_t counter;
    void* payload;
    void (*callback)(void*);
    u8_t flags;
};

/**
 * @brief Initialize the system timer that runs at specified frequency
 *
 * @param frequency The frequency that timer should run in Hz.
 */
void
timer_init();

struct lx_timer*
timer_run_second(u32_t second,
                 void (*callback)(void*),
                 void* payload,
                 u8_t flags);

struct lx_timer*
timer_run_ms(u32_t millisecond,
             void (*callback)(void*),
             void* payload,
             u8_t flags);

struct lx_timer*
timer_run(ticks_t ticks, void (*callback)(void*), void* payload, u8_t flags);

struct lx_timer_context*
timer_context();

#endif /* __LUNAIX_TIMER_H */
