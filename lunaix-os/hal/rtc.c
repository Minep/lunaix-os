/**
 * @file rtc.c
 * @author Lunaixsky
 * @brief RTC & CMOS abstraction. Reference: MC146818A & Intel Series 500 PCH datasheet
 * @version 0.1
 * @date 2022-03-07
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <hal/rtc.h>
#include <klibc/string.h>

void
rtc_init() {
    uint8_t regA = rtc_read_reg(RTC_REG_A | WITH_NMI_DISABLED);
    regA = (regA & ~0x7f) | RTC_FREQUENCY_1024HZ | RTC_DIVIDER_33KHZ;
    rtc_write_reg(RTC_REG_A | WITH_NMI_DISABLED, regA);

    // Make sure the rtc timer is disabled by default
    rtc_disable_timer();
}

uint8_t
rtc_read_reg(uint8_t reg_selector)
{
    io_outb(RTC_INDEX_PORT, reg_selector);
    return io_inb(RTC_TARGET_PORT);
}

void
rtc_write_reg(uint8_t reg_selector, uint8_t val)
{
    io_outb(RTC_INDEX_PORT, reg_selector);
    io_outb(RTC_TARGET_PORT, val);
}

uint8_t
bcd2dec(uint8_t bcd)
{
    return ((bcd & 0xF0) >> 1) + ((bcd & 0xF0) >> 3) + (bcd & 0xf);
}

int
rtc_date_same(rtc_datetime* a, rtc_datetime* b) {
    return a->year == b->year &&
           a->month == b->month &&
           a->day == b->day &&
           a->weekday == b->weekday &&
           a->minute == b->minute &&
           a->second == b->second;
}

void
rtc_get_datetime(rtc_datetime* datetime)
{
    rtc_datetime current;
    
    do
    {
        while (rtc_read_reg(RTC_REG_A) & 0x80);
        memcpy(&current, datetime, sizeof(rtc_datetime));

        datetime->year = rtc_read_reg(RTC_REG_YRS);
        datetime->month = rtc_read_reg(RTC_REG_MTH);
        datetime->day = rtc_read_reg(RTC_REG_DAY);
        datetime->weekday = rtc_read_reg(RTC_REG_WDY);
        datetime->hour = rtc_read_reg(RTC_REG_HRS);
        datetime->minute = rtc_read_reg(RTC_REG_MIN);
        datetime->second = rtc_read_reg(RTC_REG_SEC);
    } while (!rtc_date_same(datetime, &current));

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
    if (!RTC_24HRS_ENCODED(regbv) && (datetime->hour >> 7)) {
        datetime->hour = (12 + datetime->hour & 0x80);
    }

    datetime->year += RTC_CURRENT_CENTRY * 100;
}

void
rtc_enable_timer() {
    uint8_t regB = rtc_read_reg(RTC_REG_B | WITH_NMI_DISABLED);
    rtc_write_reg(RTC_REG_B | WITH_NMI_DISABLED, regB | RTC_TIMER_ON);
}

void
rtc_disable_timer() {
    uint8_t regB = rtc_read_reg(RTC_REG_B | WITH_NMI_DISABLED);
    rtc_write_reg(RTC_REG_B | WITH_NMI_DISABLED, regB & ~RTC_TIMER_ON);
}