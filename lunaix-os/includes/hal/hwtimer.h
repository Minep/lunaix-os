#ifndef __LUNAIX_HWTIMER_H
#define __LUNAIX_HWTIMER_H

#include <lunaix/device.h>
#include <lunaix/time.h>
#include <lunaix/types.h>

typedef void (*timer_tick_cb)();

struct hwtimer
{
    char* name;
    void* data;

    struct devclass class;
    struct device* timer_dev;

    int (*supported)(struct hwtimer*);
    void (*init)(struct hwtimer*, u32_t hertz, timer_tick_cb);
    ticks_t (*systicks)();
    ticks_t base_freq;
    ticks_t running_freq;
};

void
hwtimer_init(u32_t hertz, void* tick_callback);

struct hwtimer*
hwtimer_choose();

ticks_t
hwtimer_base_frequency();

ticks_t
hwtimer_current_systicks();

ticks_t
hwtimer_to_ticks(u32_t value, int unit);

#endif /* __LUNAIX_HWTIMER_H */
