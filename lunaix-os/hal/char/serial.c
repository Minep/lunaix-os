#include <lunaix/device.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>

#include <sys/mm/mempart.h>

#include <hal/serial.h>

static DEFINE_LLIST(serial_devs);
static int serial_idx = 0;

#define serial_device(dev) ((struct serial_dev*)(dev)->underlay)

int
serial_accept_one(struct serial_dev* sdev, u8_t val)
{
    return !!fifo_putone(&sdev->rxbuf, val);
}

int
serial_accept_buffer(struct serial_dev* sdev, void* val, size_t len)
{
    return !!fifo_write(&sdev->rxbuf, val, len);
}

void
serial_end_recv(struct serial_dev* sdev)
{
    pwake_one(&sdev->wq_rxdone);
}

void
serial_end_xmit(struct serial_dev* sdev, size_t len)
{
    sdev->wr_len = len;
    pwake_one(&sdev->wq_txdone);
}

int
serial_readone_nowait(struct serial_dev* sdev, u8_t* val)
{
    mutex_lock(&sdev->lock);

    int rd_len = fifo_readone(&sdev->rxbuf, val);

    mutex_unlock(&sdev->lock);

    return rd_len;
}

void
serial_readone(struct serial_dev* sdev, u8_t* val)
{
    mutex_lock(&sdev->lock);

    while (!fifo_readone(&sdev->rxbuf, val)) {
        pwait(&sdev->wq_rxdone);
    }

    mutex_unlock(&sdev->lock);
}

size_t
serial_readbuf(struct serial_dev* sdev, u8_t* buf, size_t len)
{
    mutex_lock(&sdev->lock);

    size_t rdlen;
    while (!(rdlen = fifo_read(&sdev->rxbuf, buf, len))) {
        pwait(&sdev->wq_rxdone);
    }

    mutex_unlock(&sdev->lock);

    return rdlen;
}

int
serial_readbuf_nowait(struct serial_dev* sdev, u8_t* buf, size_t len)
{
    mutex_lock(&sdev->lock);

    int rdlen = fifo_read(&sdev->rxbuf, buf, len);

    mutex_unlock(&sdev->lock);

    return rdlen;
}

int
serial_writebuf(struct serial_dev* sdev, u8_t* buf, size_t len)
{
    mutex_lock(&sdev->lock);

    if (sdev->write(sdev, buf, len) == RXTX_DONE) {
        goto done;
    }

    pwait(&sdev->wq_txdone);

done:
    int rdlen = sdev->wr_len;
    mutex_unlock(&sdev->lock);

    return rdlen;
}

int
serial_writebuf_nowait(struct serial_dev* sdev, u8_t* buf, size_t len)
{
    mutex_lock(&sdev->lock);

    sdev->write(sdev, buf, len);
    int rdlen = sdev->wr_len;

    mutex_unlock(&sdev->lock);

    return rdlen;
}

static int
__serial_read(struct device* dev, void* buf, size_t offset, size_t len)
{
    return serial_readbuf(serial_device(dev), &((u8_t*)buf)[offset], len);
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

#define RXBUF_SIZE 512

struct serial_dev*
serial_create()
{
    struct serial_dev* sdev = valloc(sizeof(struct serial_dev));
    struct device* dev = device_addseq(NULL, sdev, "ttyS%d", serial_idx++);
    dev->ops.read = __serial_read;
    dev->ops.read_page = __serial_read_page;
    dev->ops.write = __serial_write;
    dev->ops.write_page = __serial_write_page;
    dev->ops.exec_cmd = __serial_exec_command;

    sdev->dev = dev;
    dev->underlay = sdev;

    fifo_init(&sdev->rxbuf, valloc(RXBUF_SIZE), RXBUF_SIZE, 0);
    llist_append(&serial_devs, &sdev->sdev_list);
    // llist_init_head(&sdev->cmds);

    return sdev;
}

struct serial_dev*
serial_get_avilable()
{
    struct serial_dev *pos, *n;
    llist_for_each(pos, n, &serial_devs, sdev_list)
    {
        if (!mutex_on_hold(&pos->lock)) {
            return pos;
        }
    }

    return NULL;
}