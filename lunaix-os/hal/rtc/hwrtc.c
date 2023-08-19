#include <hal/hwrtc.h>

const struct hwrtc* current_rtc;

void
hwrtc_init()
{
    current_rtc = hwrtc_choose();

    current_rtc->init(current_rtc);
}

void
hwrtc_walltime(datetime_t* dt)
{
    current_rtc->get_walltime(current_rtc, dt);
}