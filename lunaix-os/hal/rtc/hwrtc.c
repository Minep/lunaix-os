#include <hal/hwrtc.h>

#include <lunaix/fs/twifs.h>
#include <lunaix/fs/twimap.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/status.h>

#include <usr/lunaix/ioctl_defs.h>

const struct hwrtc* primary_rtc;
static int rtc_count = 0;

DEFINE_LLIST(rtcs);

// void
// hwrtc_init()
// {
//     ldga_invoke_fn0(rtcdev);
// }

void
hwrtc_walltime(datetime_t* dt)
{
    primary_rtc->get_walltime(primary_rtc, dt);
}

static int
hwrtc_ioctl(struct device* dev, u32_t req, va_list args)
{
    struct hwrtc* rtc = (struct hwrtc*)dev->underlay;
    switch (req) {
        case RTCIO_IMSK:
            rtc->set_mask(rtc);
            break;
        case RTCIO_IUNMSK:
            rtc->cls_mask(rtc);
            break;
        case RTCIO_SETDT:
            datetime_t* dt = va_arg(args, datetime_t*);
            rtc->set_walltime(rtc, dt);
            break;
        case RTCIO_SETFREQ:
            ticks_t* freq = va_arg(args, ticks_t*);

            if (!freq) {
                return EINVAL;
            }
            if (*freq) {
                return rtc->chfreq(rtc, *freq);
            }

            *freq = rtc->base_freq;

            break;
        default:
            return EINVAL;
    }

    return 0;
}

static int
hwrtc_read(struct device* dev, void* buf, size_t offset, size_t len)
{
    struct hwrtc* rtc = (struct hwrtc*)dev->underlay;
    *((ticks_t*)buf) = rtc->get_counts(rtc);

    return sizeof(ticks_t);
}

struct hwrtc*
hwrtc_alloc_new(char* name)
{
    struct hwrtc* rtc_instance = valloc(sizeof(struct hwrtc));

    if (!rtc_instance) {
        return NULL;
    }

    llist_append(&rtcs, &rtc_instance->rtc_list);

    if (!primary_rtc) {
        primary_rtc = rtc_instance;
    }

    rtc_instance->name = name;
    struct device* rtcdev =
      device_addsys(NULL, rtc_instance, "rtc%d", rtc_count);

    rtcdev->ops.exec_cmd = hwrtc_ioctl;
    rtcdev->ops.read = hwrtc_read;

    rtc_instance->rtc_dev = rtcdev;

    rtc_count++;

    return rtc_instance;
}

static void
__hwrtc_readinfo(struct twimap* mapping)
{
    struct hwrtc* rtc = twimap_data(mapping, struct hwrtc*);
    twimap_printf(mapping, "device: %s\n", rtc->name);
    twimap_printf(mapping, "frequency: %dHz\n", rtc->base_freq);
    twimap_printf(mapping, "ticks count: %d\n", rtc->get_counts(rtc));
    twimap_printf(
      mapping, "ticking: %s\n", (rtc->state & RTC_STATE_MASKED) ? "no" : "yes");

    datetime_t dt;
    rtc->get_walltime(rtc, &dt);

    twimap_printf(
      mapping, "recorded date: %d/%d/%d\n", dt.year, dt.month, dt.day);
    twimap_printf(
      mapping, "recorded time: %d:%d:%d\n", dt.hour, dt.minute, dt.second);
    twimap_printf(mapping, "recorded weekday: %d\n", dt.weekday);
}

static void
hwrtc_twifs_export(struct hwrtc* rtc)
{
    const char* name = rtc->rtc_dev->name.value;
    struct twimap* rtc_mapping = twifs_mapping(NULL, rtc, name);
    rtc_mapping->read = __hwrtc_readinfo;
}

static void
hwrtc_twifs_export_all()
{
    struct hwrtc *pos, *next;
    llist_for_each(pos, next, &rtcs, rtc_list)
    {
        hwrtc_twifs_export(pos);
    }
}
EXPORT_TWIFS_PLUGIN(rtc_fsexport, hwrtc_twifs_export_all);