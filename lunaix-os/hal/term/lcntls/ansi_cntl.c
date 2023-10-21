#include <hal/term.h>

#include <lunaix/process.h>

#define CTRL_MNEMO(chr) (chr - 'A' + 1)

static inline int
__ansi_actcontrol(struct term* termdev, char chr)
{
    struct linebuffer* lbuf = &termdev->line;
    switch (chr) {
        case '\0':            // EOL
        case CTRL_MNEMO('D'): // EOF
            return 0;

        case CTRL_MNEMO('C'): // INTR
            signal_send(termdev->fggrp, SIGINT);
            break;

        case '\r': // CR
            termdev->line.ptr = 0;
            return 1;

        case '\x08': // ERASE
            return line_put_next(lbuf, chr, -1);

        case CTRL_MNEMO('Q'): // QUIT
            signal_send(termdev->fggrp, SIGKILL);
            return 1;

        case CTRL_MNEMO('Z'): // SUSP
            signal_send(termdev->fggrp, SIGSTOP);
            return 1;

        default:
            if ((int)chr < 32) {
                line_put_next(lbuf, '^', 0);
                chr += 64;
            }
            break;
    }

    return line_put_next(lbuf, chr, 0);
}

static size_t
ansi_lcntl_process(struct term* termdev, char* line, size_t len)
{
    size_t i = 0;
    while (i < len && __ansi_actcontrol(termdev, line[i])) {
        i++;
    }

    return i;
}

struct term_lcntl ansi_line_controller = { .apply = ansi_lcntl_process };