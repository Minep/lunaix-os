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
#include "lcntl.h"
#include <usr/lunaix/term.h>

#define CTRL_MNEMO(chr) (chr - 'A' + 1)

int
__ansi_actcontrol(struct lcntl_state* state, char chr)
{
    if (chr < 32 && chr != '\n') {
        lcntl_put_char(state, '^');
        return lcntl_put_char(state, chr += 64);
    }

    return lcntl_put_char(state, chr);
}
