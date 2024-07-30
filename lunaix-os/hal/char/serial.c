#include <lunaix/device.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/owloysius.h>
#include <lunaix/status.h>
#include <lunaix/syslog.h>

#include <sys/mm/pagetable.h>

#include <hal/serial.h>
#include <hal/term.h>

LOG_MODULE("serial")

#define lock_sdev(sdev) device_lock((sdev)->dev)
#define unlock_sdev(sdev) device_unlock((sdev)->dev)
#define unlock_and_wait(sdev, wq)                                              \
    ({                                                                         \
        unlock_sdev(sdev);                                                     \
        pwait(&(sdev)->wq);                                                    \
        lock_sdev(sdev);                                                       \
    })

static DEFINE_LLIST(serial_devs);
static int serial_idx = 0;

static struct device_cat* serial_cat;

#define serial_device(dev) ((struct serial_dev*)(dev)->underlay)

int
serial_accept_one(struct serial_dev* sdev, u8_t val)
{
    return !!rbuffer_put(&sdev->rxbuf, val);
}

int
serial_accept_buffer(struct serial_dev* sdev, void* val, size_t len)
{
    return !!rbuffer_puts(&sdev->rxbuf, val, len);
}

void
serial_end_recv(struct serial_dev* sdev)
{
    mark_device_done_read(sdev->dev);

    pwake_one(&sdev->wq_rxdone);

    struct termport_capability* tpcap;
    tpcap = get_capability(sdev->tp_cap, typeof(*tpcap));
    term_notify_data_avaliable(tpcap);
}

void
serial_end_xmit(struct serial_dev* sdev, size_t len)
{
    mark_device_done_write(sdev->dev);

    sdev->wr_len = len;
    pwake_one(&sdev->wq_txdone);
}

int
serial_readone_nowait(struct serial_dev* sdev, u8_t* val)
{
    lock_sdev(sdev);

    int rd_len = rbuffer_get(&sdev->rxbuf, (char*)val);

    unlock_sdev(sdev);

    return rd_len;
}

void
serial_readone(struct serial_dev* sdev, u8_t* val)
{
    lock_sdev(sdev);

    mark_device_doing_read(sdev->dev);

    while (!rbuffer_get(&sdev->rxbuf, (char*)val)) {
        unlock_and_wait(sdev, wq_rxdone);
    }

    unlock_sdev(sdev);
}

size_t
serial_readbuf(struct serial_dev* sdev, u8_t* buf, size_t len)
{
    lock_sdev(sdev);

    mark_device_doing_read(sdev->dev);

    size_t rdlen;
    while (!(rdlen = rbuffer_gets(&sdev->rxbuf, (char*)buf, len))) {
        unlock_and_wait(sdev, wq_rxdone);
    }

    unlock_sdev(sdev);

    return rdlen;
}

int
serial_readbuf_nowait(struct serial_dev* sdev, u8_t* buf, size_t len)
{
    lock_sdev(sdev);

    mark_device_doing_read(sdev->dev);

    int rdlen = rbuffer_gets(&sdev->rxbuf, (char*)buf, len);

    unlock_sdev(sdev);

    return rdlen;
}

int
serial_writebuf(struct serial_dev* sdev, u8_t* buf, size_t len)
{
    lock_sdev(sdev);

    mark_device_doing_write(sdev->dev);

    if (sdev->write(sdev, buf, len) == RXTX_DONE) {
        goto done;
    }

    unlock_and_wait(sdev, wq_txdone);

done:
    int rdlen = sdev->wr_len;
    unlock_sdev(sdev);

    return rdlen;
}

int
serial_writebuf_nowait(struct serial_dev* sdev, u8_t* buf, size_t len)
{
    lock_sdev(sdev);

    mark_device_doing_write(sdev->dev);

    sdev->write(sdev, buf, len);
    int rdlen = sdev->wr_len;

    unlock_sdev(sdev);

    return rdlen;
}

static int
__serial_read(struct device* dev, void* buf, off_t fpos, size_t len)
{
    return serial_readbuf(serial_device(dev), (u8_t*)buf, len);
}

static int
__serial_read_async(struct device* dev, void* buf, off_t fpos, size_t len)
{
    return serial_readbuf_nowait(
        serial_device(dev), (u8_t*)buf, len);
}

static int
__serial_read_page(struct device* dev, void* buf, off_t fpos)
{
    return serial_readbuf(serial_device(dev), (u8_t*)buf, PAGE_SIZE);
}

static int
__serial_write(struct device* dev, void* buf, off_t fpos, size_t len)
{
    return serial_writebuf(serial_device(dev), (u8_t*)buf, len);
}

static int
__serial_write_async(struct device* dev, void* buf, off_t fpos, size_t len)
{
    return serial_writebuf_nowait(
        serial_device(dev), (u8_t*)buf, len);
}

static int
__serial_write_page(struct device* dev, void* buf, off_t fpos)
{
    return serial_writebuf(serial_device(dev), (u8_t*)buf, PAGE_SIZE);
}

static int
__serial_exec_command(struct device* dev, u32_t req, va_list args)
{
    struct serial_dev* sdev = serial_device(dev);

    if (!sdev->exec_cmd) {
        return ENOTSUP;
    }

    return sdev->exec_cmd(sdev, req, args);
}

static int
__serial_poll_event(struct device* dev)
{
    struct serial_dev* sdev = serial_device(dev);

    return sdev->dev->poll_evflags;
}

static void sdev_execmd(struct serial_dev* sdev, u32_t req, ...)
{
    va_list args;
    va_start(args, req);

    sdev->exec_cmd(sdev, req, args);

    va_end(args);
}

static void
__serial_set_speed(struct device* dev, speed_t speed)
{
    struct serial_dev* sdev = serial_device(dev);
    lock_sdev(sdev);

    sdev_execmd(sdev, SERIO_SETBRDRATE, speed);

    unlock_sdev(sdev);
}

static void
__serial_set_baseclk(struct device* dev, unsigned int base)
{
    struct serial_dev* sdev = serial_device(dev);
    lock_sdev(sdev);

    sdev_execmd(sdev, SERIO_SETBRDBASE, base);

    unlock_sdev(sdev);
}

static void
__serial_set_cntrl_mode(struct device* dev, tcflag_t cflag)
{
    struct serial_dev* sdev = serial_device(dev);
    lock_sdev(sdev);

    sdev_execmd(sdev, SERIO_SETCNTRLMODE, cflag);

    unlock_sdev(sdev);
}

#define RXBUF_SIZE 512

static struct termport_cap_ops tpcap_ops = {
    .set_cntrl_mode = __serial_set_cntrl_mode,
    .set_clkbase = __serial_set_baseclk,
    .set_speed = __serial_set_speed
};

struct serial_dev*
serial_create(struct devclass* class, char* if_ident)
{
    struct serial_dev* sdev = vzalloc(sizeof(struct serial_dev));
    struct device* dev = device_allocseq(dev_meta(serial_cat), sdev);
    dev->ops.read = __serial_read;
    dev->ops.read_page = __serial_read_page;
    dev->ops.read_async = __serial_read_async;
    dev->ops.write_async = __serial_write_async;
    dev->ops.write = __serial_write;
    dev->ops.write_page = __serial_write_page;
    dev->ops.exec_cmd = __serial_exec_command;
    dev->ops.poll = __serial_poll_event;
    
    sdev->dev = dev;
    dev->underlay = sdev;

    struct termport_capability* tp_cap = 
        new_capability(TERMPORT_CAP, struct termport_capability);
    
    term_cap_set_operations(tp_cap, &tpcap_ops);
    sdev->tp_cap = cap_meta(tp_cap);

    waitq_init(&sdev->wq_rxdone);
    waitq_init(&sdev->wq_txdone);
    rbuffer_init(&sdev->rxbuf, valloc(RXBUF_SIZE), RXBUF_SIZE);
    llist_append(&serial_devs, &sdev->sdev_list);
    
    device_grant_capability(dev, cap_meta(tp_cap));

    register_device(dev, class, "%s%d", if_ident, class->variant);

    term_create(dev, if_ident);

    INFO("interface: %s, %xh:%xh.%d", dev->name_val, 
            class->fn_grp, class->device, class->variant);

    class->variant++;
    return sdev;
}

struct serial_dev*
serial_get_avilable()
{
    struct serial_dev *pos, *n;
    llist_for_each(pos, n, &serial_devs, sdev_list)
    {
        if (!device_locked(pos->dev)) {
            return pos;
        }
    }

    return NULL;
}

static void
init_serial_dev()
{
    serial_cat = device_addcat(NULL, "serial");

    assert(serial_cat);
}
owloysius_fetch_init(init_serial_dev, on_earlyboot)