#ifndef __LUNAIX_HWRTC_H
#define __LUNAIX_HWRTC_H

#include <lunaix/time.h>

struct hwrtc
{
    char* name;
    void* data;

    ticks_t base_freq;

    int (*supported)(struct hwrtc*);
    void (*init)(struct hwrtc*);

    void (*get_walltime)(struct hwrtc*, datetime_t*);

    void (*do_ticking)(struct hwrtc*, void (*on_tick)());
    void (*end_ticking)(struct hwrtc*);
};

extern const struct hwrtc* current_rtc;

void
hwrtc_init();

struct hwrtc*
hwrtc_choose();

void
hwrtc_walltime(datetime_t* dt);

#endif /* __LUNAIX_HWRTC_H */
