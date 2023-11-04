#ifndef __LUNAIX_CLOCK_H
#define __LUNAIX_CLOCK_H

#include <lunaix/time.h>

#include <hal/hwrtc.h>
#include <hal/hwtimer.h>

extern const struct hwrtc* sysrtc;
extern const struct hwtimer* systimer;

void
clock_init();

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

#endif /* __LUNAIX_CLOCK_H */
