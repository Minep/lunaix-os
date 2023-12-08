#include <lunaix/device.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>

#include <sys/mm/mempart.h>

#include <hal/serial.h>

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
__serial_read(struct device* dev, void* buf, size_t offset, size_t len)
{
    return serial_readbuf(serial_device(dev), &((u8_t*)buf)[offset], len);
}

static int
__serial_read_async(struct device* dev, void* buf, size_t offset, size_t len)
{
    return serial_readbuf_nowait(
        serial_device(dev), &((u8_t*)buf)[offset], len);
}

static int
__serial_read_page(struct device* dev, void* buf, size_t offset)
{
    return serial_readbuf(serial_device(dev), &((u8_t*)buf)[offset], MEM_PAGE);
}

static int
__serial_write(struct device* dev, void* buf, size_t offset, size_t len)
{
    return serial_writebuf(serial_device(dev), &((u8_t*)buf)[offset], len);
}

static int
__serial_write_async(struct device* dev, void* buf, size_t offset, size_t len)
{
    return serial_writebuf_nowait(
        serial_device(dev), &((u8_t*)buf)[offset], len);
}

static int
__serial_write_page(struct device* dev, void* buf, size_t offset)
{
    return serial_writebuf(serial_device(dev), &((u8_t*)buf)[offset], MEM_PAGE);
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

#define RXBUF_SIZE 512

struct serial_dev*
serial_create(struct devclass* class, char* if_ident)
{
    struct serial_dev* sdev = valloc(sizeof(struct serial_dev));
    struct device* dev = device_allocseq(NULL, sdev);
    dev->ops.read = __serial_read;
    dev->ops.read_page = __serial_read_page;
    dev->ops.write = __serial_write;
    dev->ops.write_page = __serial_write_page;
    dev->ops.exec_cmd = __serial_exec_command;
    dev->ops.poll = __serial_poll_event;

    sdev->dev = dev;
    dev->underlay = sdev;

    waitq_init(&sdev->wq_rxdone);
    waitq_init(&sdev->wq_txdone);
    rbuffer_init(&sdev->rxbuf, valloc(RXBUF_SIZE), RXBUF_SIZE);
    llist_append(&serial_devs, &sdev->sdev_list);

    device_register(dev, class, "port%s%d", if_ident, class->variant);

    sdev->at_term = term_create(dev, if_ident);

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