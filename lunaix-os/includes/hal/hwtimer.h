#ifndef __LUNAIX_HWTIMER_H
#define __LUNAIX_HWTIMER_H

#include <lunaix/device.h>
#include <lunaix/time.h>
#include <lunaix/types.h>

typedef void (*timer_tick_cb)();

#define HWTIMER_MIN_PRECEDENCE      0
#define HWTIMER_MAX_PRECEDENCE      15

struct hwtimer_pot;
struct hwtimer_pot_ops
{
    void (*calibrate)(struct hwtimer_pot*, u32_t hertz);
};

struct hwtimer_pot
{
    POTENS_META;
    
    struct llist_header timers;
    
    int precedence;
    timer_tick_cb callback;
    ticks_t base_freq;
    ticks_t running_freq;
    volatile ticks_t systick_raw;

    struct hwtimer_pot_ops* ops;
};

void
hwtimer_init(u32_t hertz, void* tick_callback);

ticks_t
hwtimer_current_systicks();

ticks_t
hwtimer_to_ticks(u32_t value, int unit);

struct hwtimer_pot*
hwtimer_attach_potens(struct device* dev, int precedence, 
                      struct hwtimer_pot_ops* ops);


#endif /* __LUNAIX_HWTIMER_H */
