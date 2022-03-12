#ifndef __LUNAIX_RTC_H
#define __LUNAIX_RTC_H

#include "io.h"

#define RTC_INDEX_PORT 0x70
#define RTC_TARGET_PORT 0x71

#define WITH_NMI_DISABLED 0x80

#define RTC_CURRENT_CENTRY 20

#define RTC_REG_YRS 0x9
#define RTC_REG_MTH 0x8
#define RTC_REG_DAY 0x7
#define RTC_REG_WDY 0x6
#define RTC_REG_HRS 0x4
#define RTC_REG_MIN 0x2
#define RTC_REG_SEC 0x0

#define RTC_REG_A 0xA
#define RTC_REG_B 0xB
#define RTC_REG_C 0xC
#define RTC_REG_D 0xD

#define RTC_BIN_ENCODED(reg)    (reg & 0x04)
#define RTC_24HRS_ENCODED(reg)  (reg & 0x02)

#define RTC_TIMER_BASE_FREQUENCY    1024
#define RTC_TIMER_ON                0x40

#define RTC_FREQUENCY_1024HZ    0b110
#define RTC_DIVIDER_33KHZ       (0b010 << 4)

void
rtc_init();

uint8_t
rtc_read_reg(uint8_t reg_selector);

void
rtc_write_reg(uint8_t reg_selector, uint8_t val);

void
rtc_enable_timer();

void
rtc_disable_timer();

#endif /* __LUNAIX_RTC_H */
