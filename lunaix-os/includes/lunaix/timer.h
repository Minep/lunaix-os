#ifndef __LUNAIX_TIMER_H
#define __LUNAIX_TIMER_H

#include <lunaix/ds/llist.h>
#include <stdint.h>

#define SYS_TIMER_FREQUENCY_HZ      2048UL

#define TIMER_MODE_PERIODIC   0x1

struct lx_timer_context {
    struct lx_timer *active_timers;
    uint32_t base_frequency;
    uint32_t running_frequency;
    uint32_t tick_interval;
};

struct lx_timer {
    struct llist_header link;
    uint32_t deadline;
    uint32_t counter;
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
timer_init(uint32_t frequency);

int
timer_run_second(uint32_t second, void (*callback)(void*), void* payload, uint8_t flags);

int
timer_run(uint32_t ticks, void (*callback)(void*), void* payload, uint8_t flags);

#endif /* __LUNAIX_TIMER_H */
