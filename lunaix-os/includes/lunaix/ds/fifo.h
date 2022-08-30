#ifndef __LUNAIX_FIFO_BUF_H
#define __LUNAIX_FIFO_BUF_H

#include <lunaix/ds/mutex.h>
#include <lunaix/types.h>

#define FIFO_DIRTY 1

struct fifo_buf
{
    void* data;
    size_t wr_pos;
    size_t rd_pos;
    size_t size;
    size_t free_len;
    size_t flags;
    mutex_t lock;
};

int
fifo_backone(struct fifo_buf* fbuf);

size_t
fifo_putone(struct fifo_buf* fbuf, uint8_t data);

size_t
fifo_readone_async(struct fifo_buf* fbuf, uint8_t* data);

void
fifo_set_rdptr(struct fifo_buf* fbuf, size_t rdptr);

void
fifo_set_wrptr(struct fifo_buf* fbuf, size_t wrptr);

void
fifo_init(struct fifo_buf* buf, void* data_buffer, size_t buf_size, int flags);

size_t
fifo_write(struct fifo_buf* fbuf, void* data, size_t count);

size_t
fifo_read(struct fifo_buf* fbuf, void* buf, size_t count);

void
fifo_clear(struct fifo_buf* fbuf);

#endif /* __LUNAIX_FIFO_BUF_H */
