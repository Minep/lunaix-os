#include <lunaix/clock.h>
#include <lunaix/device.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/spike.h>

#include <klibc/string.h>

void
__clock_read_systime(struct twimap* map)
{
    ticks_t sys_time = clock_systime();
    twimap_printf(map, "%u", sys_time);
}

void
__clock_read_datetime(struct twimap* map)
{
    datetime_t dt;
    clock_walltime(&dt);
    twimap_printf(map,
                  "%.4d-%.2d-%.2d %.2d:%.2d:%.2d",
                  dt.year, dt.month, dt.day,
                  dt.hour, dt.minute, dt.second);
}

void
__clock_read_unix(struct twimap* map)
{
    twimap_printf(map, "%u", clock_unixtime());
}

void
clock_build_mapping()
{
    struct twifs_node* root = twifs_dir_node(NULL, "clock");
    struct twimap* map;

    map = twifs_mapping(root, NULL, "systime");
    map->read = __clock_read_systime;

    map = twifs_mapping(root, NULL, "unix");
    map->read = __clock_read_unix;

    map = twifs_mapping(root, NULL, "datetime");
    map->read = __clock_read_datetime;
}
EXPORT_TWIFS_PLUGIN(sys_clock, clock_build_mapping);

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