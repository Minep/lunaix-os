#include <lunaix/clock.h>
#include <lunaix/device.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/spike.h>

#include <klibc/string.h>

static void
__twimap_read_systime(struct twimap* map)
{
    ticks_t sys_time = clock_systime();
    twimap_printf(map, "%u", sys_time);
}

static void
__twimap_read_datetime(struct twimap* map)
{
    datetime_t dt;
    clock_walltime(&dt);
    twimap_printf(map,
                  "%.4d-%.2d-%.2d %.2d:%.2d:%.2d",
                  dt.year, dt.month, dt.day,
                  dt.hour, dt.minute, dt.second);
}

static void
__twimap_read_unixtime(struct twimap* map)
{
    twimap_printf(map, "%u", clock_unixtime());
}

time_t
clock_unixtime()
{
    datetime_t dt;
    hwrtc_walltime(&dt);
    return datetime_tounix(&dt);
}

time_t
clock_systime()
{
    if (!systimer) {
        return 0;
    }

    ticks_t t = hwtimer_current_systicks();
    ticks_t tu = systimer->running_freq / 1000;

    if (unlikely(!tu)) {
        return t;
    }

    return t / (tu);
}

void
clock_walltime(datetime_t* datetime)
{
    sysrtc->ops->get_walltime(sysrtc, datetime);
}

static void
clock_build_mapping()
{
    struct twifs_node* root;
    struct twimap* map;

    root = twifs_dir_node(NULL, "clock");
    
    twimap_export_value(root, systime, FSACL_ugR, NULL);
    twimap_export_value(root, unixtime, FSACL_ugR, NULL);
    twimap_export_value(root, datetime, FSACL_ugR, NULL);
}
EXPORT_TWIFS_PLUGIN(sys_clock, clock_build_mapping);
