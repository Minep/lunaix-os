#ifndef __LUNAIX_CLOCK_H
#define __LUNAIX_CLOCK_H

#include <lunaix/time.h>

#include <hal/hwrtc.h>
#include <hal/hwtimer.h>

extern const struct hwrtc_potens* sysrtc;
extern const struct hwtimer_pot* systimer;

void
clock_walltime(datetime_t* datetime);

/**
 * @brief 返回当前系统时间，即自从开机到当前时刻的毫秒时。
 *
 * @return time_t
 */
time_t
clock_systime();

time_t
clock_unixtime();

static inline void
clock_init()
{
    hwrtc_init();
}

#endif /* __LUNAIX_CLOCK_H */
