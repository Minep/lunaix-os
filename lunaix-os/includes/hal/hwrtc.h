#ifndef __LUNAIX_HWRTC_H
#define __LUNAIX_HWRTC_H

#include <lunaix/device.h>
#include <lunaix/ds/ldga.h>
#include <lunaix/ds/llist.h>
#include <lunaix/time.h>

#define RTC_STATE_MASKED 0x1

#define EXPORT_RTC_DEVICE(id, init_fn)                                         \
    export_ldga_el(rtcdev, id, ptr_t, init_fn)

struct hwrtc
{
    struct llist_header rtc_list;
    struct device* rtc_dev;

    char* name;
    void* data;
    ticks_t base_freq;
    int state;

    void (*get_walltime)(struct hwrtc*, datetime_t*);
    void (*set_walltime)(struct hwrtc*, datetime_t*);
    void (*set_mask)(struct hwrtc*);
    void (*cls_mask)(struct hwrtc*);
    int (*get_counts)(struct hwrtc*);
    int (*chfreq)(struct hwrtc*, int);
};

extern const struct hwrtc* primary_rtc;

void
hwrtc_init();

struct hwrtc*
hwrtc_alloc_new(char* driver_id);

void
hwrtc_walltime(datetime_t* dt);

#endif /* __LUNAIX_HWRTC_H */
