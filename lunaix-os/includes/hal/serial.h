#ifndef __LUNAIX_SERIAL_H
#define __LUNAIX_SERIAL_H

#include <lunaix/device.h>
#include <lunaix/ds/fifo.h>
#include <lunaix/ds/llist.h>
#include <lunaix/ds/mutex.h>
#include <lunaix/ds/waitq.h>

#define SERIAL_RW_RX 0x0
#define SERIAL_RW_TX 0x1
#define io_dir(flags) ((flags) & SERIAL_RW_TX)

#define RXTX_DONE 0x1
#define RXTX_WAIT 0x2

#define SERIAL_AGAIN 0x1
#define SERIAL_DONE 0x0

struct serial_dev;
typedef int (*rxtx_cb)(struct serial_dev*);

struct serial_dev
{
    struct llist_header sdev_list;
    struct device* dev;
    mutex_t lock;
    struct waitq wq_rxdone;
    struct waitq wq_txdone;
    void* backend;

    struct fifo_buf rxbuf;
    int wr_len;

    /**
     * @brief Write buffer to TX. The return code indicate
     * the transaction is either done in synced mode (TX_DONE) or will be
     * done asynchronously (TX_WAIT).
     *
     */
    int (*write)(struct serial_dev* sdev, u8_t*, size_t);
    int (*exec_cmd)(struct serial_dev* sdev, u32_t, va_list);
};

struct serial_dev*
serial_create();

void
serial_readone(struct serial_dev* sdev, u8_t* val);

int
serial_readone_nowait(struct serial_dev* sdev, u8_t* val);

size_t
serial_readbuf(struct serial_dev* sdev, u8_t* buf, size_t len);

int
serial_readbuf_nowait(struct serial_dev* sdev, u8_t* buf, size_t len);

int
serial_writebuf(struct serial_dev* sdev, u8_t* buf, size_t len);

int
serial_writebuf_nowait(struct serial_dev* sdev, u8_t* buf, size_t len);

struct serial_dev*
serial_get_avilable();

int
serial_accept_one(struct serial_dev* sdev, u8_t val);

int
serial_accept_buffer(struct serial_dev* sdev, void* val, size_t len);

void
serial_end_recv(struct serial_dev* sdev);

void
serial_end_xmit(struct serial_dev* sdev, size_t len);

#endif /* __LUNAIX_SERIAL_H */
