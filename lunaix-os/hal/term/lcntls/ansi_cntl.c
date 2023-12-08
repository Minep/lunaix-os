/**
 * @file ansi_cntl.c
 * @author Lunaixsky (lunaxisky@qq.com)
 * @brief Line controller slave that handle all non-POSIX control code or ANSI
 * escape sequence
 * @version 0.1
 * @date 2023-11-25
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <hal/term.h>
#include <usr/lunaix/term.h>

#define CTRL_MNEMO(chr) (chr - 'A' + 1)

static inline int
__ansi_actcontrol(struct term* termdev, struct linebuffer* lbuf, char chr)
{
    struct rbuffer* cooked = lbuf->next;
    switch (chr) {
        default:
            if ((int)chr < 32) {
                rbuffer_put(cooked, '^');
                chr += 64;
            }
            break;
    }

    return rbuffer_put(cooked, chr);
}

struct term_lcntl ansi_line_controller = {.process_and_put = __ansi_actcontrol};