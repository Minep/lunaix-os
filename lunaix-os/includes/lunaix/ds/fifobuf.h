#ifndef __LUNAIX_FIFO_BUF_H
#define __LUNAIX_FIFO_BUF_H

#include <lunaix/ds/mutex.h>

#define FIFO_DIRTY 1

struct fifo_buffer
{
    void* data;
    unsigned int wr_pos;
    unsigned int rd_pos;
    unsigned int size;
    unsigned int flags;
    mutex_t lock;
};

#endif /* __LUNAIX_FIFO_BUF_H */
