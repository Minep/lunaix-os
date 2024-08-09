#ifndef __LUNAIX_TIME_H
#define __LUNAIX_TIME_H

#include <lunaix/types.h>

#define TIME_MS -1000
#define TIME_SEC 1
#define TIME_MIN (TIME_SEC * 60)
#define TIME_HOUR (TIME_MIN * 60)

typedef unsigned int ticks_t;
typedef u32_t time_t;

typedef struct
{
    u32_t year; // use int32 as we need to store the 4-digit year
    u8_t month;
    u8_t day;
    u8_t weekday;
    u8_t hour;
    u8_t minute;
    u8_t second;
} datetime_t;

static inline ticks_t
ticks_seconds(unsigned int seconds)
{
    return seconds * 1000;
}

static inline ticks_t
ticks_minutes(unsigned int min)
{
    return ticks_seconds(min * 60);
}

static inline ticks_t
ticks_msecs(unsigned int ms)
{
    return ms;
}

static inline time_t
datetime_tounix(datetime_t* dt)
{
    return (dt->year - 1970) * 31556926u + (dt->month - 1) * 2629743u +
           (dt->day - 1) * 86400u + (dt->hour - 1) * 3600u +
           (dt->minute - 1) * 60u + dt->second;
}

static inline time_t
time_tounix(u32_t yyyy, u32_t mm, u32_t dd, u32_t hh, u32_t MM, u32_t ss)
{
    return (yyyy - 1970) * 31556926u + (mm - 1) * 2629743u + (dd - 1) * 86400u +
           (hh - 1) * 3600u + (MM - 1) * 60u + ss;
}

static inline int
datatime_eq(datetime_t* a, datetime_t* b)
{
    return a->year == b->year && a->month == b->month && a->day == b->day &&
           a->weekday == b->weekday && a->minute == b->minute &&
           a->second == b->second;
}

#endif /* __LUNAIX_TIME_H */
