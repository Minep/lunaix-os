#ifndef __LUNAIX_TIMER_H
#define __LUNAIX_TIMER_H

#include <lunaix/ds/llist.h>
#include <stdint.h>

#define SYS_TIMER_FREQUENCY_HZ 1024

#define TIMER_MODE_PERIODIC 0x1

typedef u32_t ticks_t;

struct lx_timer_context
{
    struct lx_timer* active_timers;
    /**
     * @brief APIC timer base frequency (ticks per seconds)
     *
     */
    ticks_t base_frequency;
    /**
     * @brief Desired system running frequency
     *
     */
    u32_t running_frequency;
    /**
     * @brief Ticks per hertz
     *
     */
    ticks_t tphz;
};

struct lx_timer
{
    struct llist_header link;
    ticks_t deadline;
    ticks_t counter;
    void* payload;
    void (*callback)(void*);
    uint8_t flags;
};

/**
 * @brief Initialize the system timer that runs at specified frequency
 *
 * @param frequency The frequency that timer should run in Hz.
 */
void
timer_init(u32_t frequency);

struct lx_timer*
timer_run_second(u32_t second,
                 void (*callback)(void*),
                 void* payload,
                 uint8_t flags);

struct lx_timer*
timer_run_ms(u32_t millisecond,
             void (*callback)(void*),
             void* payload,
             uint8_t flags);

struct lx_timer*
timer_run(ticks_t ticks, void (*callback)(void*), void* payload, uint8_t flags);

struct lx_timer_context*
timer_context();

#endif /* __LUNAIX_TIMER_H */
