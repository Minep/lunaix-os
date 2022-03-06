#include <hal/rtc.h>

uint8_t
rtc_read_reg(uint8_t reg_selector)
{
    io_outb(RTC_INDEX_PORT, reg_selector);
    return io_inb(RTC_TARGET_PORT);
}

uint8_t
bcd2dec(uint8_t bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0x0f);
}

void
rtc_get_datetime(rtc_datetime* datetime)
{
    datetime->year = rtc_read_reg(RTC_REG_YRS);
    datetime->month = rtc_read_reg(RTC_REG_MTH);
    datetime->day = rtc_read_reg(RTC_REG_DAY);
    datetime->weekday = rtc_read_reg(RTC_REG_WDY);
    datetime->hour = rtc_read_reg(RTC_REG_HRS);
    datetime->minute = rtc_read_reg(RTC_REG_MIN);
    datetime->second = rtc_read_reg(RTC_REG_SEC);

    uint8_t regbv = rtc_read_reg(RTC_REG_B);

    // Convert from bcd to binary when needed
    if (!RTC_BIN_ENCODED(regbv)) {
        datetime->year = bcd2dec(datetime->year);
        datetime->month = bcd2dec(datetime->month);
        datetime->day = bcd2dec(datetime->day);
        datetime->hour = bcd2dec(datetime->hour);
        datetime->minute = bcd2dec(datetime->minute);
        datetime->second = bcd2dec(datetime->second);
    }


    // To 24 hour format
    if (!RTC_24HRS_ENCODED(regbv)) {
        datetime->hour = (datetime->hour >> 7) ? (12 + datetime->hour & 0x80)
                                               : (datetime->hour & 0x80);
    }

    datetime->year += RTC_CURRENT_CENTRY * 100;
}