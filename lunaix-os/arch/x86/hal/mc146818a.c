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
    struct hwrtc_potens* rtc_context;
    u32_t rtc_iv;
};

#define rtc_state(data) ((struct mc146818*)(data))

static inline u8_t
rtc_read_reg(u8_t reg_selector)
{
    port_wrbyte(RTC_INDEX_PORT, reg_selector);
    return port_rdbyte(RTC_TARGET_PORT);
}

static inline void
rtc_write_reg(u8_t reg_selector, u8_t val)
{
    port_wrbyte(RTC_INDEX_PORT, reg_selector);
    port_wrbyte(RTC_TARGET_PORT, val);
}

static inline void
rtc_enable_timer()
{
    u8_t regB = rtc_read_reg(RTC_REG_B);
    rtc_write_reg(RTC_REG_B, regB | RTC_TIMER_ON);
}

static inline void
rtc_disable_timer()
{
    u8_t regB = rtc_read_reg(RTC_REG_B);
    rtc_write_reg(RTC_REG_B, regB & ~RTC_TIMER_ON);
}

static void
rtc_getwalltime(struct hwrtc_potens* rtc, datetime_t* datetime)
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
rtc_setwalltime(struct hwrtc_potens* rtc, datetime_t* datetime)
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

static void
__rtc_tick(const struct hart_state* hstate)
{
    struct mc146818* state = (struct mc146818*)isrm_get_payload(hstate);

    state->rtc_context->live++;

    (void)rtc_read_reg(RTC_REG_C);
}

static void
rtc_set_proactive(struct hwrtc_potens* pot, bool proactive)
{
    if (proactive) {
        pot->state = 0;
        rtc_enable_timer();
    }
    else {
        pot->state = RTC_STATE_MASKED;
        rtc_disable_timer();
    }
}

static int
rtc_chfreq(struct hwrtc_potens* rtc, int freq)
{
    return ENOTSUP;
}

static int
__rtc_calibrate(struct hwrtc_potens* pot)
{
    struct mc146818* state;

    u8_t reg = rtc_read_reg(RTC_REG_A);
    reg = (reg & ~0x7f) | RTC_FREQUENCY_1024HZ | RTC_DIVIDER_33KHZ;
    rtc_write_reg(RTC_REG_A, reg);

    reg = RTC_BIN | RTC_HOURFORM24;
    rtc_write_reg(RTC_REG_B, reg);

    // Make sure the rtc timer is disabled by default
    rtc_disable_timer();

    pot->base_freq = RTC_TIMER_BASE_FREQUENCY;

    state = (struct mc146818*)potens_dev(pot)->underlay;
    state->rtc_iv = isrm_bindirq(PC_AT_IRQ_RTC, __rtc_tick);
    isrm_set_payload(state->rtc_iv, __ptr(state));

    return 0;
}

static struct hwrtc_potens_ops ops = {
    .get_walltime  = rtc_getwalltime,
    .set_walltime  = rtc_setwalltime,
    .set_proactive = rtc_set_proactive,
    .chfreq = rtc_chfreq,
    .calibrate = __rtc_calibrate
};

static int
rtc_load(struct device_def* devdef)
{
    struct device* dev;
    struct mc146818* state;
    struct hwrtc_potens* pot;
 
    state = valloc(sizeof(struct mc146818));
    dev = device_allocsys(NULL, state);

    pot = hwrtc_attach_potens(dev, &ops);
    if (!pot) {
        return 1;
    }

    pot->state = RTC_STATE_MASKED;    
    state->rtc_context = pot;

    register_device(dev, &devdef->class, "mc146818");

    return 0;
}

static struct device_def devrtc_mc146818 = {
    def_device_class(SOC, TIME, RTC_86LEGACY),
    def_device_name("x86 legacy RTC"),
    def_on_load(rtc_load)
};
EXPORT_DEVICE(mc146818, &devrtc_mc146818, load_sysconf);