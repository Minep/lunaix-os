#include <lunaix/spike.h>
#include <lunaix/time.h>

#include <hal/hwtimer.h>

const struct hwtimer_pot* systimer = NULL;

static struct device_alias* timer_alias;
static DEFINE_LLIST(timer_devices);

ticks_t
hwtimer_base_frequency()
{
    assert(systimer);
    return systimer->base_freq;
}

ticks_t
hwtimer_current_systicks()
{
    assert(systimer);
    return systimer->systick_raw;
}

ticks_t
hwtimer_to_ticks(u32_t value, int unit)
{
    assert(systimer);
    
    // in case system frequency is less than 1000Hz
    if (unit != TIME_MS) {
        return systimer->running_freq * unit * value;
    }

    ticks_t freq_ms = systimer->running_freq / 1000;

    return freq_ms * value;
}

static struct hwtimer_pot*
__select_timer()
{
    struct hwtimer_pot *pos, *n, *sel = NULL;

    llist_for_each(pos, n, &timer_devices, timers)
    {
        if (unlikely(!sel)) {
            sel = pos;
            continue;
        }

        if (sel->precedence < pos->precedence) {
            sel = pos;
        }
    }

    return sel;
}

void
hwtimer_init(u32_t hertz, void* tick_callback)
{
    struct hwtimer_pot* selected;
    struct device* time_dev;

    selected = __select_timer();

    assert(selected);
    systimer = selected;

    selected->callback = tick_callback;
    selected->running_freq = hertz;

    selected->ops->calibrate(selected, hertz);

    time_dev = potens_dev(selected);
    timer_alias = device_addalias(NULL, dev_meta(time_dev), "timer");
}


struct hwtimer_pot*
hwtimer_attach_potens(struct device* dev, int precedence, 
                      struct hwtimer_pot_ops* ops)
{
    struct hwtimer_pot* hwpot;
    struct potens_meta* pot;

    if (!potens_check_unique(dev, potens(HWTIMER)))
    {
        return NULL;
    }

    hwpot = new_potens(potens(HWTIMER), struct hwtimer_pot);
    hwpot->ops = ops;
    hwpot->precedence = precedence;
    
    device_grant_potens(dev, potens_meta(hwpot));

    llist_append(&timer_devices, &hwpot->timers);

    return hwpot;
}