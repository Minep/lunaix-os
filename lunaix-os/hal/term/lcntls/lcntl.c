/**
 * @file lcntl.c
 * @author Lunaixsky (lunaxisky@qq.com)
 * @brief Line controller master that handle all POSIX-control code
 * @version 0.1
 * @date 2023-11-25
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <hal/term.h>
#include <usr/lunaix/term.h>

#include "lcntl.h"

#include <lunaix/process.h>
#include <lunaix/spike.h>

static inline void
init_lcntl_state(struct lcntl_state* state, 
                 struct term* tdev, enum lcntl_dir direction)
{
    *state = (struct lcntl_state) {
        .tdev = tdev,
        ._lf  = tdev->lflags,
        ._cf  = tdev->cflags,
        .direction = direction,
        .echobuf = tdev->line_out.current,
    };

    struct linebuffer* lb;
    if (direction == INBOUND) {
        state->_if = tdev->iflags;
        lb = &tdev->line_in;
    }
    else {
        state->_of = tdev->oflags;
        lb = &tdev->line_out;
    }

    state->inbuf = lb->current;
    state->outbuf = lb->next;
    state->active_line = lb;
}

static inline void
raise_sig(struct lcntl_state* state, int sig)
{
    term_sendsig(state->tdev, sig);
    lcntl_raise_line_event(state, LEVT_SIGRAISE);
}

static inline char
__remap_character(struct lcntl_state* state, char c)
{
    if (c == '\r') {
        if ((state->_if & _ICRNL) || (state->_of & _OCRNL)) {
            return '\n';
        }
    } 
    else if (c == '\n') {
        if ((state->_if & _INLCR) || (state->_of & (_ONLRET))) {
            return '\r';
        }
    }
    else if ('a' <= c && c <= 'z') {
        if ((state->_if & _IUCLC)) {
            return c | 0b100000;
        } else if ((state->_of & _OLCUC)) {
            return c & ~0b100000;
        }
    }

    return c;
}

static inline void
lcntl_echo_char(struct lcntl_state* state, char c)
{
    rbuffer_put(state->echobuf, c);
}

int
__ansi_actcontrol(struct lcntl_state* state, char chr);

static inline int must_inline
lcntl_transform_seq(struct lcntl_state* state)
{
    struct term* tdev = state->tdev;
    char c;
    int i = 0;
    bool full = false;

#define EOL tdev->cc[_VEOL]
#define EOF tdev->cc[_VEOF]
#define ERASE tdev->cc[_VERASE]
#define KILL tdev->cc[_VKILL]
#define INTR tdev->cc[_VINTR]
#define QUIT tdev->cc[_VQUIT]
#define SUSP tdev->cc[_VSUSP]

    while (!lcntl_test_flag(state, LCNTLF_STOP)) 
    {
        lcntl_unset_flag(state, LCNTLF_SPECIAL_CHAR);

        if (!rbuffer_get(state->inbuf, &c)) {
            break;
        }

        c = __remap_character(state, c);

        if (c == '\0') {
            if ((state->_if & _IGNBRK)) {
                continue;
            }

            if ((state->_if & _BRKINT)) {
                raise_sig(state, SIGINT);
                break;
            }
        }

        if (lcntl_outbound(state)) {
            goto do_out;
        }

        if (c == '\n') {
            if (lcntl_check_echo(state, _ECHONL)) {
                lcntl_echo_char(state, c);
            }
        }

        // For input procesing

        lcntl_set_flag(state, LCNTLF_SPECIAL_CHAR);

        if (c == '\n' || c == EOL) {
            lcntl_raise_line_event(state, LEVT_EOL);
        }
        else if (c == EOF) {
            lcntl_raise_line_event(state, LEVT_EOF);
            lcntl_set_flag(state, LCNTLF_CLEAR_INBUF);
            lcntl_set_flag(state, LCNTLF_STOP);
        }
        else if (c == INTR) {
            raise_sig(state, SIGINT);
            lcntl_set_flag(state, LCNTLF_CLEAR_OUTBUF);
        }
        else if (c == QUIT) {
            raise_sig(state, SIGKILL);
            lcntl_set_flag(state, LCNTLF_CLEAR_OUTBUF);
            lcntl_set_flag(state, LCNTLF_STOP);
        }
        else if (c == SUSP) {
            raise_sig(state, SIGSTOP);
        }
        else if (c == ERASE) {
            if (rbuffer_erase(state->outbuf) && 
                lcntl_check_echo(state, _ECHOE)) 
            {
                lcntl_echo_char(state, '\x8'); 
                lcntl_echo_char(state, ' '); 
                lcntl_echo_char(state, '\x8');
            }
            continue;
        }
        else if (c == KILL) {
            lcntl_set_flag(state, LCNTLF_CLEAR_OUTBUF);
        }
        else {
            lcntl_unset_flag(state, LCNTLF_SPECIAL_CHAR);
        }

        if (lcntl_check_echo(state, _ECHOK) && c == KILL) {
            lcntl_echo_char(state, c);
            lcntl_echo_char(state, '\n');
        }

    do_out:
        if (c == '\n' && (state->_of & _ONLCR)) {
            full = !rbuffer_put_nof(state->outbuf, '\r');
        }

        if (!full) {
            if (lcntl_inbound(state) && (state->_lf & _IEXTEN)) {
                full = !__ansi_actcontrol(state, c);
            }
            else {
                full = !lcntl_put_char(state, c);
            }
        }

        if (lcntl_test_flag(state, LCNTLF_CLEAR_INBUF)) {
            rbuffer_clear(state->inbuf);
            lcntl_unset_flag(state, LCNTLF_CLEAR_INBUF);
        }
        
        if (lcntl_test_flag(state, LCNTLF_CLEAR_OUTBUF)) {
            rbuffer_clear(state->outbuf);
            lcntl_unset_flag(state, LCNTLF_CLEAR_OUTBUF);
        }

        i++;
    }

    if (state->direction != OUTBOUND && !(state->_lf & _NOFLSH)) {
        term_flush(tdev);
    }

    line_flip(state->active_line);

    return i;
}

int
lcntl_transform_inseq(struct term* tdev)
{
    struct lcntl_state state;

    init_lcntl_state(&state, tdev, INBOUND);
    return lcntl_transform_seq(&state);
}

int
lcntl_transform_outseq(struct term* tdev)
{
    struct lcntl_state state;

    init_lcntl_state(&state, tdev, OUTBOUND);
    return lcntl_transform_seq(&state);
}

int 
lcntl_put_char(struct lcntl_state* state, char c)
{
    if (lcntl_check_echo(state, _ECHO)) {
        lcntl_echo_char(state, c);
    }

    if (!lcntl_test_flag(state, LCNTLF_SPECIAL_CHAR)) {
        return rbuffer_put_nof(state->outbuf, c);
    }

    return 1;
}