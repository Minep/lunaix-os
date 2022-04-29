#ifndef __LUNAIX_CLOCK_H
#define __LUNAIX_CLOCK_H

#include <stdint.h>

typedef uint32_t time_t;

typedef struct
{
    uint32_t year;      // use int32 as we need to store the 4-digit year
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

#endif /* __LUNAIX_CLOCK_H */
