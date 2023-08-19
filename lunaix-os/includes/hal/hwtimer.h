#ifndef __LUNAIX_HWTIMER_H
#define __LUNAIX_HWTIMER_H

#include <lunaix/time.h>
#include <lunaix/types.h>

typedef void (*timer_tick_cb)();

struct hwtimer_context
{
    char* name;
    void* data;

    int (*supported)(struct hwtimer_context*);
    void (*init)(struct hwtimer_context*, u32_t hertz, timer_tick_cb);
    ticks_t (*systicks)();
    ticks_t base_freq;
    ticks_t running_freq;
};

void
hwtimer_init(u32_t hertz, void* tick_callback);

struct hwtimer_context*
hwtimer_choose();

ticks_t
hwtimer_base_frequency();

ticks_t
hwtimer_current_systicks();

ticks_t
hwtimer_to_ticks(u32_t value, int unit);

#endif /* __LUNAIX_HWTIMER_H */
