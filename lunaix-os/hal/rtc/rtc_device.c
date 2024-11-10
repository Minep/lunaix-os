#include <lunaix/fs/twifs.h>
#include <lunaix/fs/twimap.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/status.h>
#include <lunaix/syslog.h>

#include <hal/hwrtc.h>

const struct hwrtc_potens* sysrtc = NULL;

static DEFINE_LLIST(rtcs);

LOG_MODULE("hwrtc")

void
hwrtc_walltime(datetime_t* dt)
{
    sysrtc->ops->get_walltime(sysrtc, dt);
}

static inline struct hwrtc_potens*
__rtc_potens(struct device* wrapper)
{
    struct potens_meta* pot;
    pot = device_get_potens(wrapper, potens(HWRTC));
    return ({ assert(pot); get_potens(pot, struct hwrtc_potens); });
}

static int
__hwrtc_ioctl(struct device* dev, u32_t req, va_list args)
{
    struct hwrtc_potens* pot;
    struct hwrtc_potens_ops* ops;
    struct device* rtcdev;

    rtcdev = (struct device*) dev->underlay;
    pot = __rtc_potens(rtcdev);
    ops = pot->ops;

    switch (req) {
        case RTCIO_IMSK:
            ops->set_proactive(pot, false);
            break;
        case RTCIO_IUNMSK:
            ops->set_proactive(pot, true);
            break;
        case RTCIO_SETDT:
            datetime_t* dt = va_arg(args, datetime_t*);
            ops->set_walltime(pot, dt);
            break;
        case RTCIO_SETFREQ:
            ticks_t* freq = va_arg(args, ticks_t*);

            if (!freq) {
                return EINVAL;
            }
            if (*freq) {
                return ops->chfreq(pot, *freq);
            }

            *freq = pot->base_freq;

            break;
        default:
            return rtcdev->ops.exec_cmd(dev, req, args);
    }

    return 0;
}

static int
__hwrtc_read(struct device* dev, void* buf, size_t offset, size_t len)
{
    struct hwrtc_potens* pot;

    pot = __rtc_potens((struct device*)dev->underlay);
    *((ticks_t*)buf) = pot->live;

    return sizeof(ticks_t);
}

static struct devclass proxy_rtc_clas = DEVCLASS(LUNAIX, TIME, RTC);

static void
__hwrtc_create_proxy(struct hwrtc_potens* pot, struct device* raw_rtcdev)
{
    struct device* dev;

    dev = device_allocsys(NULL, raw_rtcdev);

    dev->ops.exec_cmd = __hwrtc_ioctl;
    dev->ops.read = __hwrtc_read;

    register_device_var(dev, &proxy_rtc_clas, "rtc");

    pot->rtc_proxy = dev;
}

struct hwrtc_potens*
hwrtc_attach_potens(struct device* raw_rtcdev, struct hwrtc_potens_ops* ops)
{
    struct hwrtc_potens* hwpot;

    if (!potens_check_unique(raw_rtcdev, potens(HWRTC)))
    {
        return NULL;
    }

    hwpot = new_potens(potens(HWRTC), struct hwrtc_potens);
    hwpot->ops = ops;
    
    device_grant_potens(raw_rtcdev, potens_meta(hwpot));
    llist_append(&rtcs, &hwpot->rtc_potentes);

    __hwrtc_create_proxy(hwpot, raw_rtcdev);

    return hwpot;
}

void
hwrtc_init()
{
    assert(!llist_empty(&rtcs));

    sysrtc = list_entry(rtcs.next, struct hwrtc_potens, rtc_potentes);
    
    if (!sysrtc->ops->calibrate) {
        return;
    }

    int err = sysrtc->ops->calibrate(sysrtc);
    if (err) {
        FATAL("failed to calibrate rtc. name='%s', err=%d", 
                potens_dev(sysrtc)->name_val, err);
    }
}

static void
__hwrtc_readinfo(struct twimap* mapping)
{
    struct hwrtc_potens* pot;
    struct device* owner;

    pot = twimap_data(mapping, struct hwrtc_potens*);
    owner = pot->pot_meta.owner;

    twimap_printf(mapping, "device: %x.%x\n", 
                    owner->ident.fn_grp, owner->ident.unique);

    twimap_printf(mapping, "frequency: %dHz\n", pot->base_freq);
    twimap_printf(mapping, "ticks count: %d\n", pot->live);
    twimap_printf(mapping, "ticking: %s\n",
                  (pot->state & RTC_STATE_MASKED) ? "no" : "yes");

    datetime_t dt;
    pot->ops->get_walltime(pot, &dt);

    twimap_printf(
        mapping, "recorded date: %d/%d/%d\n", dt.year, dt.month, dt.day);
    twimap_printf(
        mapping, "recorded time: %d:%d:%d\n", dt.hour, dt.minute, dt.second);
    twimap_printf(mapping, "recorded weekday: %d\n", dt.weekday);
}

static void
hwrtc_twifs_export(struct hwrtc_potens* pot)
{
    const char* name = pot->rtc_proxy->name_val;
    struct twimap* rtc_mapping = twifs_mapping(NULL, pot, name);
    rtc_mapping->read = __hwrtc_readinfo;
}

static void
hwrtc_twifs_export_all()
{
    struct hwrtc_potens *pos, *next;
    llist_for_each(pos, next, &rtcs, rtc_potentes)
    {
        hwrtc_twifs_export(pos);
    }
}
EXPORT_TWIFS_PLUGIN(rtc_fsexport, hwrtc_twifs_export_all);
