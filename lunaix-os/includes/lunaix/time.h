#ifndef __LUNAIX_TIME_H
#define __LUNAIX_TIME_H

#include <stdint.h>

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
time_getdatetime(datetime_t* datetime);

#endif /* __LUNAIX_TIME_H */
