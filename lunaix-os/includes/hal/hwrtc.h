#ifndef __LUNAIX_HWRTC_H
#define __LUNAIX_HWRTC_H

#include <lunaix/device.h>
#include <lunaix/ds/ldga.h>
#include <lunaix/ds/llist.h>
#include <lunaix/time.h>

#define RTC_STATE_MASKED 0x1

struct hwrtc_potens;
struct hwrtc_potens_ops
{
    void (*get_walltime)(struct hwrtc_potens*, datetime_t*);
    void (*set_walltime)(struct hwrtc_potens*, datetime_t*);
    void (*set_proactive)(struct hwrtc_potens*, bool);
    int  (*chfreq)(struct hwrtc_potens*, int);

    int (*calibrate)(struct hwrtc_potens*);
};

struct hwrtc_potens
{
    POTENS_META;

    struct llist_header rtc_potentes;
    struct device* rtc_proxy;

    ticks_t base_freq;
    volatile ticks_t live;
    int state;

    struct hwrtc_potens_ops* ops;
};

void
hwrtc_init();

void
hwrtc_walltime(datetime_t* dt);

struct hwrtc_potens*
hwrtc_attach_potens(struct device* raw_rtcdev, struct hwrtc_potens_ops* ops);

#endif /* __LUNAIX_HWRTC_H */
