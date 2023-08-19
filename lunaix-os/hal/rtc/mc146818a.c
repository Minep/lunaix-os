/**
 * @file rtc.c
 * @author Lunaixsky
 * @brief RTC & CMOS abstraction. Reference: MC146818A & Intel Series 500 PCH
 * datasheet
 * @version 0.1
 * @date 2022-03-07
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <lunaix/isrm.h>

#include <hal/rtc/mc146818a.h>

#include <klibc/string.h>

#include <sys/interrupts.h>
#include <sys/port_io.h>

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

#define RTC_BIN_ENCODED(reg) (reg & 0x04)
#define RTC_24HRS_ENCODED(reg) (reg & 0x02)

#define RTC_TIMER_BASE_FREQUENCY 1024
#define RTC_TIMER_ON 0x40

#define RTC_FREQUENCY_1024HZ 0b110
#define RTC_DIVIDER_33KHZ (0b010 << 4)

struct mc146818
{
    struct hwrtc* rtc_context;
    u32_t rtc_iv;
    int ticking;
    void (*on_tick_cb)(const struct hwrtc*);
};

#define rtc_state(data) ((struct mc146818*)(data))

static u8_t
rtc_read_reg(u8_t reg_selector)
{
    port_wrbyte(RTC_INDEX_PORT, reg_selector);
    return port_rdbyte(RTC_TARGET_PORT);
}

static void
rtc_write_reg(u8_t reg_selector, u8_t val)
{
    port_wrbyte(RTC_INDEX_PORT, reg_selector);
    port_wrbyte(RTC_TARGET_PORT, val);
}

static u8_t
bcd2dec(u8_t bcd)
{
    return ((bcd & 0xF0) >> 1) + ((bcd & 0xF0) >> 3) + (bcd & 0xf);
}

static void
rtc_enable_timer()
{
    u8_t regB = rtc_read_reg(RTC_REG_B | WITH_NMI_DISABLED);
    rtc_write_reg(RTC_REG_B | WITH_NMI_DISABLED, regB | RTC_TIMER_ON);
}

static void
rtc_disable_timer()
{
    u8_t regB = rtc_read_reg(RTC_REG_B | WITH_NMI_DISABLED);
    rtc_write_reg(RTC_REG_B | WITH_NMI_DISABLED, regB & ~RTC_TIMER_ON);
}

static void
clock_walltime(struct hwrtc* rtc, datetime_t* datetime)
{
    datetime_t current;

    do {
        while (rtc_read_reg(RTC_REG_A) & 0x80)
            ;
        memcpy(&current, datetime, sizeof(datetime_t));

        datetime->year = rtc_read_reg(RTC_REG_YRS);
        datetime->month = rtc_read_reg(RTC_REG_MTH);
        datetime->day = rtc_read_reg(RTC_REG_DAY);
        datetime->weekday = rtc_read_reg(RTC_REG_WDY);
        datetime->hour = rtc_read_reg(RTC_REG_HRS);
        datetime->minute = rtc_read_reg(RTC_REG_MIN);
        datetime->second = rtc_read_reg(RTC_REG_SEC);
    } while (!datatime_eq(datetime, &current));

    u8_t regbv = rtc_read_reg(RTC_REG_B);

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
        datetime->hour = 12 + (datetime->hour & 0x80);
    }

    datetime->year += RTC_CURRENT_CENTRY * 100;
}

static int
mc146818_check_support(struct hwrtc* rtc)
{
#if __ARCH__ == i386
    return 1;
#else
    return 0;
#endif
}

static void
rtc_init(struct hwrtc* rtc)
{
    u8_t regA = rtc_read_reg(RTC_REG_A | WITH_NMI_DISABLED);
    regA = (regA & ~0x7f) | RTC_FREQUENCY_1024HZ | RTC_DIVIDER_33KHZ;
    rtc_write_reg(RTC_REG_A | WITH_NMI_DISABLED, regA);

    rtc_state(rtc->data)->rtc_context = rtc;

    // Make sure the rtc timer is disabled by default
    rtc_disable_timer();
}

static struct mc146818 rtc_state = { .ticking = 0 };

static void
__rtc_tick(const isr_param* param)
{
    rtc_state.on_tick_cb(rtc_state.rtc_context);

    (void)rtc_read_reg(RTC_REG_C);
}

static void
rtc_do_ticking(struct hwrtc* rtc, void (*on_tick)())
{
    if (!on_tick || rtc_state(rtc->data)->ticking) {
        return;
    }

    struct mc146818* state = rtc_state(rtc->data);
    state->ticking = 1;
    state->on_tick_cb = on_tick;

    /* We realise that using rtc to tick something has an extremely rare use
     * case (e.g., calibrating some timer). Therefore, we will release this
     * allocated IV when rtc ticking is no longer required to save IV
     * resources.
     */
    state->rtc_iv = isrm_bindirq(PC_AT_IRQ_RTC, __rtc_tick);

    rtc_enable_timer();
}

static void
rtc_end_ticking(struct hwrtc* rtc)
{
    if (!rtc_state(rtc->data)->ticking) {
        return;
    }

    rtc_disable_timer();

    // do some delay, ensure there is no more interrupt from rtc before we
    // release isr
    port_delay(1000);

    isrm_ivfree(rtc_state(rtc->data)->rtc_iv);

    rtc_state(rtc->data)->ticking = 0;
}

static struct hwrtc hwrtc_mc146818a = { .name = "mc146818a",
                                        .data = &rtc_state,
                                        .init = rtc_init,
                                        .base_freq = RTC_TIMER_BASE_FREQUENCY,
                                        .supported = mc146818_check_support,
                                        .get_walltime = clock_walltime,
                                        .do_ticking = rtc_do_ticking,
                                        .end_ticking = rtc_end_ticking };

struct hwrtc*
mc146818a_rtc_context()
{
    return &hwrtc_mc146818a;
}
