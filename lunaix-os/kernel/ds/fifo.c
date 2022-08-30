#include <klibc/string.h>
#include <lunaix/ds/fifo.h>
#include <lunaix/ds/mutex.h>
#include <lunaix/spike.h>

void
fifo_init(struct fifo_buf* buf, void* data_buffer, size_t buf_size, int flags)
{
    *buf = (struct fifo_buf){ .data = data_buffer,
                              .rd_pos = 0,
                              .wr_pos = 0,
                              .size = buf_size,
                              .flags = flags,
                              .free_len = buf_size };
    mutex_init(&buf->lock);
}

void
fifo_clear(struct fifo_buf* fbuf)
{
    mutex_lock(&fbuf->lock);
    fbuf->rd_pos = 0;
    fbuf->wr_pos = 0;
    fbuf->free_len = fbuf->size;
    mutex_unlock(&fbuf->lock);
}

int
fifo_backone(struct fifo_buf* fbuf)
{
    mutex_lock(&fbuf->lock);

    if (fbuf->free_len == fbuf->size) {
        mutex_unlock(&fbuf->lock);
        return 0;
    }

    fbuf->wr_pos = (fbuf->wr_pos ? fbuf->wr_pos : fbuf->size) - 1;
    fbuf->free_len++;

    mutex_unlock(&fbuf->lock);

    return 1;
}

size_t
fifo_putone(struct fifo_buf* fbuf, uint8_t data)
{
    mutex_lock(&fbuf->lock);

    if (!fbuf->free_len) {
        mutex_unlock(&fbuf->lock);
        return 0;
    }

    uint8_t* dest = fbuf->data;
    dest[fbuf->wr_pos] = data;
    fbuf->wr_pos = (fbuf->wr_pos + 1) % fbuf->size;
    fbuf->free_len--;

    mutex_unlock(&fbuf->lock);

    return 1;
}

size_t
fifo_readone_async(struct fifo_buf* fbuf, uint8_t* data)
{
    if (fbuf->free_len == fbuf->size) {
        return 0;
    }

    uint8_t* dest = fbuf->data;
    *data = dest[fbuf->rd_pos];
    fbuf->rd_pos = (fbuf->rd_pos + 1) % fbuf->size;
    fbuf->free_len++;

    return 1;
}

void
fifo_set_rdptr(struct fifo_buf* fbuf, size_t rdptr)
{
    fbuf->rd_pos = rdptr;
    if (rdptr <= fbuf->wr_pos) {
        fbuf->free_len = fbuf->size - fbuf->wr_pos + rdptr;
    } else {
        fbuf->free_len = rdptr - fbuf->wr_pos;
    }
}

void
fifo_set_wrptr(struct fifo_buf* fbuf, size_t wrptr)
{
    fbuf->wr_pos = wrptr;
    if (wrptr <= fbuf->rd_pos) {
        fbuf->free_len = fbuf->size - fbuf->rd_pos + wrptr;
    } else {
        fbuf->free_len = wrptr - fbuf->rd_pos;
    }
}

size_t
fifo_write(struct fifo_buf* fbuf, void* data, size_t count)
{
    size_t wr_count = 0, wr_pos = fbuf->wr_pos;

    mutex_lock(&fbuf->lock);

    if (!fbuf->free_len) {
        mutex_unlock(&fbuf->lock);
        return 0;
    }

    if (wr_pos >= fbuf->rd_pos) {
        // case 1
        size_t cplen_tail = MIN(fbuf->size - wr_pos, count);
        size_t cplen_head = MIN(fbuf->rd_pos, count - cplen_tail);
        memcpy(fbuf->data + wr_pos, data, cplen_tail);
        memcpy(fbuf->data, data + cplen_tail, cplen_head);

        wr_count = cplen_head + cplen_tail;
    } else {
        // case 2
        wr_count = MIN(fbuf->rd_pos - wr_pos, count);
        memcpy(fbuf->data + wr_pos, data, wr_count);
    }

    fbuf->wr_pos = (wr_pos + wr_count) % fbuf->size;
    fbuf->free_len -= wr_count;

    mutex_unlock(&fbuf->lock);

    return wr_count;
}

size_t
fifo_read(struct fifo_buf* fbuf, void* buf, size_t count)
{
    size_t rd_count = 0, rd_pos = fbuf->rd_pos;
    mutex_lock(&fbuf->lock);

    if (fbuf->free_len == fbuf->size) {
        mutex_unlock(&fbuf->lock);
        return 0;
    }

    if (rd_pos >= fbuf->wr_pos) {
        size_t cplen_tail = MIN(fbuf->size - rd_pos, count);
        size_t cplen_head = MIN(fbuf->wr_pos, count - cplen_tail);
        memcpy(buf, fbuf->data + rd_pos, cplen_tail);
        memcpy(buf + cplen_tail, fbuf->data, cplen_head);

        rd_count = cplen_head + cplen_tail;
    } else {
        rd_count = MIN(fbuf->wr_pos - rd_pos, count);
        memcpy(buf, fbuf->data + rd_pos, rd_count);
    }

    fbuf->rd_pos = (rd_pos + rd_count) % fbuf->size;
    fbuf->free_len += rd_count;

    mutex_unlock(&fbuf->lock);

    return rd_count;
}