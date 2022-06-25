#ifndef __LUNAIX_CONSOLE_H
#define __LUNAIX_CONSOLE_H

#include <lunaix/ds/fifobuf.h>
#include <lunaix/timer.h>

struct console
{
    struct lx_timer* flush_timer;
    struct fifo_buffer buffer;
    unsigned int erd_pos;
    unsigned int lines;
    unsigned int chars;
};

#endif /* __LUNAIX_CONSOLE_H */
