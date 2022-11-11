#include <lunaix/clock.h>
#include <lunaix/input.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>

#include <klibc/string.h>

static DEFINE_LLIST(listener_chain);

static struct device* input_devcat = NULL;

void
input_init()
{
    input_devcat = device_addcat(NULL, "input");
}

void
input_fire_event(struct input_device* idev, struct input_evt_pkt* pkt)
{
    pkt->timestamp = clock_systime();
    idev->current_pkt = *pkt;

    struct input_evt_chain *pos, *n;
    llist_for_each(pos, n, &listener_chain, chain)
    {
        if (pos->evt_cb(idev) == INPUT_EVT_CATCH) {
            break;
        }
    }

    // wake up all pending readers
    pwake_all(&idev->readers);
}

void
input_add_listener(input_evt_cb listener)
{
    assert(listener);

    struct input_evt_chain* chain = vzalloc(sizeof(*chain));
    llist_append(&listener_chain, &chain->chain);

    chain->evt_cb = listener;
}

int
__input_dev_read(struct device* dev, void* buf, size_t offset, size_t len)
{
    struct input_device* idev = dev->underlay;

    if (len < sizeof(struct input_evt_pkt)) {
        return ERANGE;
    }

    // wait for new event
    pwait(&idev->readers);

    memcpy(buf, &idev->current_pkt, sizeof(struct input_evt_pkt));

    return sizeof(struct input_evt_pkt);
}

int
__input_dev_read_pg(struct device* dev, void* buf, size_t offset)
{
    return __input_dev_read(dev, buf, offset, PG_SIZE);
}

struct input_device*
input_add_device(char* name_fmt, ...)
{
    assert(input_devcat);

    struct input_device* idev = vzalloc(sizeof(*idev));
    waitq_init(&idev->readers);

    va_list args;
    va_start(args, name_fmt);

    struct device* dev =
      device_add(input_devcat, idev, name_fmt, DEV_IFSEQ, args);

    idev->dev_if = dev;
    dev->read = __input_dev_read;
    dev->read_page = __input_dev_read_pg;

    va_end(args);

    return idev;
}
