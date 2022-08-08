#ifndef __LUNAIX_CONSOLE_H
#define __LUNAIX_CONSOLE_H

#include <lunaix/ds/fifo.h>
#include <lunaix/timer.h>

struct console
{
    struct lx_timer* flush_timer;
    struct fifo_buf output;
    struct fifo_buf input;
    unsigned int erd_pos;
    unsigned int lines;
};

#endif /* __LUNAIX_CONSOLE_H */
