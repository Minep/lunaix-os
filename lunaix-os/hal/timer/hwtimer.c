#include <hal/hwtimer.h>
#include <lunaix/spike.h>

struct hwtimer_context* sys_hwtctx;

void
hwtimer_init(u32_t hertz, void* tick_callback)
{
    struct hwtimer_context* hwt_ctx = hwtimer_choose();

    hwt_ctx->init(hwt_ctx, hertz, tick_callback);
    hwt_ctx->running_freq = hertz;

    sys_hwtctx = hwt_ctx;
}

ticks_t
hwtimer_base_frequency()
{
    return sys_hwtctx->base_freq;
}

ticks_t
hwtimer_current_systicks()
{
    return sys_hwtctx->systicks();
}

ticks_t
hwtimer_to_ticks(u32_t value, int unit)
{
    // in case system frequency is less than 1000Hz
    if (unit != TIME_MS) {
        return sys_hwtctx->running_freq * unit * value;
    }

    ticks_t freq_ms = sys_hwtctx->running_freq / 1000;

    return freq_ms * value;
}