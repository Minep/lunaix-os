#ifndef __LUNAIX_CLOCK_H
#define __LUNAIX_CLOCK_H

#include <stdint.h>

typedef uint32_t time_t;

typedef struct
{
    uint32_t year; // use int32 as we need to store the 4-digit year
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} datetime_t;

void
clock_init();

void
clock_walltime(datetime_t* datetime);

int
clock_datatime_eq(datetime_t* a, datetime_t* b);

/**
 * @brief 返回当前系统时间，即自从开机到当前时刻的毫秒时。
 *
 * @return time_t
 */
time_t
clock_systime();

time_t
clock_unixtime();

static inline time_t
clock_tounixtime(datetime_t* dt)
{
    return (dt->year - 1970) * 31556926u + (dt->month - 1) * 2629743u +
           (dt->day - 1) * 86400u + (dt->hour - 1) * 3600u +
           (dt->minute - 1) * 60u + dt->second;
}

#endif /* __LUNAIX_CLOCK_H */
