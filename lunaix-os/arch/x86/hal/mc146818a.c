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

#include <lunaix/mm/valloc.h>
#include <lunaix/status.h>
#include <lunaix/hart_state.h>

#include <hal/hwrtc.h>

#include <klibc/string.h>

#include <asm/x86_isrm.h>
#include <asm/x86_pmio.h>

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

#define RTC_SET 0x80
#define RTC_BIN (1 << 2)
#define RTC_HOURFORM24 (1 << 1)

#define RTC_BIN_ENCODED(reg) (reg & 0x04)
#define RTC_24HRS_ENCODED(reg) (reg & 0x02)

#define RTC_TIMER_BASE_FREQUENCY 1024
#define RTC_TIMER_ON 0x40

#define RTC_FREQUENCY_1024HZ 0b110
#define RTC_DIVIDER_33KHZ (0b010 << 4)

#define PC_AT_IRQ_RTC                   8

struct mc146818
{
    struct hwrtc* rtc_context;
    u32_t rtc_iv;
    u32_t tick_counts;
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

static void
rtc_enable_timer()
{
    u8_t regB = rtc_read_reg(RTC_REG_B);
    rtc_write_reg(RTC_REG_B, regB | RTC_TIMER_ON);
}

static void
rtc_disable_timer()
{
    u8_t regB = rtc_read_reg(RTC_REG_B);
    rtc_write_reg(RTC_REG_B, regB & ~RTC_TIMER_ON);
}

static void
rtc_getwalltime(struct hwrtc* rtc, datetime_t* datetime)
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

    datetime->year += RTC_CURRENT_CENTRY * 100;
}

static void
rtc_setwalltime(struct hwrtc* rtc, datetime_t* datetime)
{
    u8_t reg = rtc_read_reg(RTC_REG_B);
    reg = reg & ~RTC_SET;

    rtc_write_reg(RTC_REG_B, reg | RTC_SET);

    rtc_write_reg(RTC_REG_YRS, datetime->year - RTC_CURRENT_CENTRY * 100);
    rtc_write_reg(RTC_REG_MTH, datetime->month);
    rtc_write_reg(RTC_REG_DAY, datetime->day);
    rtc_write_reg(RTC_REG_WDY, datetime->weekday);
    rtc_write_reg(RTC_REG_HRS, datetime->hour);
    rtc_write_reg(RTC_REG_MIN, datetime->minute);
    rtc_write_reg(RTC_REG_SEC, datetime->second);

    rtc_write_reg(RTC_REG_B, reg & ~RTC_SET);
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
__rtc_tick(const struct hart_state* hstate)
{
    struct mc146818* state = (struct mc146818*)isrm_get_payload(hstate);

    state->tick_counts++;

    (void)rtc_read_reg(RTC_REG_C);
}

static void
rtc_set_mask(struct hwrtc* rtc)
{
    rtc->state = RTC_STATE_MASKED;
    rtc_disable_timer();
}

static void
rtc_cls_mask(struct hwrtc* rtc)
{
    struct mc146818* state = rtc_state(rtc->data);

    rtc->state = 0;
    state->tick_counts = 0;
    rtc_enable_timer();
}

static int
rtc_chfreq(struct hwrtc* rtc, int freq)
{
    return ENOTSUP;
}

static int
rtc_getcnt(struct hwrtc* rtc)
{
    struct mc146818* state = rtc_state(rtc->data);
    return state->tick_counts;
}

static int
rtc_init(struct device_def* devdef)
{
    u8_t reg = rtc_read_reg(RTC_REG_A);
    reg = (reg & ~0x7f) | RTC_FREQUENCY_1024HZ | RTC_DIVIDER_33KHZ;
    rtc_write_reg(RTC_REG_A, reg);

    reg = RTC_BIN | RTC_HOURFORM24;
    rtc_write_reg(RTC_REG_B, reg);

    // Make sure the rtc timer is disabled by default
    rtc_disable_timer();

    struct hwrtc* rtc = hwrtc_alloc_new("mc146818");
    struct mc146818* state = valloc(sizeof(struct mc146818));

    state->rtc_context = rtc;
    state->rtc_iv = isrm_bindirq(PC_AT_IRQ_RTC, __rtc_tick);
    isrm_set_payload(state->rtc_iv, (ptr_t)state);

    rtc->state = RTC_STATE_MASKED;
    rtc->data = state;
    rtc->base_freq = RTC_TIMER_BASE_FREQUENCY;
    rtc->get_walltime = rtc_getwalltime;
    rtc->set_walltime = rtc_setwalltime;
    rtc->set_mask = rtc_set_mask;
    rtc->cls_mask = rtc_cls_mask;
    rtc->get_counts = rtc_getcnt;
    rtc->chfreq = rtc_chfreq;

    hwrtc_register(&devdef->class, rtc);

    return 0;
}

static struct device_def devrtc_mc146818 = {
    .name = "MC146818 RTC",
    .class = DEVCLASS(DEVIF_SOC, DEVFN_TIME, DEV_RTC),
    .init = rtc_init
};
EXPORT_DEVICE(mc146818, &devrtc_mc146818, load_timedev);