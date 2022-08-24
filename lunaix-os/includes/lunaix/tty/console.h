#ifndef __LUNAIX_CONSOLE_H
#define __LUNAIX_CONSOLE_H

#include <lunaix/ds/fifo.h>
#include <lunaix/timer.h>

struct console
{
    struct lx_timer* flush_timer;
    struct fifo_buf output;
    struct fifo_buf input;
    size_t wnd_start;
    size_t lines;
};

#endif /* __LUNAIX_CONSOLE_H */
