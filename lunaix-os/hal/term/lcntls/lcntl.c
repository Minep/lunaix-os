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

#include <lunaix/process.h>
#include <lunaix/spike.h>

static inline void
raise_sig(struct term* at_term, struct linebuffer* lbuf, int sig)
{
    term_sendsig(at_term, sig);
    lbuf->sflags |= LSTATE_SIGRAISE;
}

static inline int must_inline optimize("-fipa-cp-clone")
lcntl_transform_seq(struct term* tdev, struct linebuffer* lbuf, bool out)
{
    struct rbuffer* raw = lbuf->current;
    struct rbuffer* cooked = lbuf->next;
    struct rbuffer* output = tdev->line_out.current;

    int i = 0, _if = tdev->iflags & -!out, _of = tdev->oflags & -!!out,
        _lf = tdev->lflags;
    int allow_more = 1, latest_eol = cooked->ptr;
    char c;

    int (*lcntl_slave_put)(struct term*, struct linebuffer*, char) =
        tdev->lcntl->process_and_put;

#define EOL tdev->cc[_VEOL]
#define EOF tdev->cc[_VEOF]
#define ERASE tdev->cc[_VERASE]
#define KILL tdev->cc[_VKILL]
#define INTR tdev->cc[_VINTR]
#define QUIT tdev->cc[_VQUIT]
#define SUSP tdev->cc[_VSUSP]

#define putc_safe(rb, chr)                  \
    ({                                      \
        if (!rbuffer_put_nof(rb, chr)) {    \
            break;                          \
        }                                   \
    })

    if (!out) {
        // Keep all cc's (except VMIN & VTIME) up to L2 cache.
        for (size_t i = 0; i < _VMIN; i++) {
            prefetch_rd(&tdev->cc[i], 2);
        }
    }

    while (rbuffer_get(raw, &c)) {

        if (c == '\r' && ((_if & _ICRNL) || (_of & _OCRNL))) {
            c = '\n';
        } else if (c == '\n') {
            if ((_if & _INLCR) || (_of & (_ONLRET))) {
                c = '\r';
            }
        }

        if (c == '\0') {
            if ((_if & _IGNBRK)) {
                continue;
            }

            if ((_if & _BRKINT)) {
                raise_sig(tdev, lbuf, SIGINT);
                break;
            }
        }

        if ('a' <= c && c <= 'z') {
            if ((_if & _IUCLC)) {
                c = c | 0b100000;
            } else if ((_of & _OLCUC)) {
                c = c & ~0b100000;
            }
        }

        if (out) {
            goto do_out;
        }

        if (c == '\n') {
            latest_eol = cooked->ptr + 1;
            if ((_lf & _ECHONL)) {
                rbuffer_put(output, c);
            }
        }

        // For input procesing

        if (c == '\n' || c == EOL) {
            lbuf->sflags |= LSTATE_EOL;
        } else if (c == EOF) {
            lbuf->sflags |= LSTATE_EOF;
            rbuffer_clear(raw);
            break;
        } else if (c == INTR) {
            raise_sig(tdev, lbuf, SIGINT);
        } else if (c == QUIT) {
            raise_sig(tdev, lbuf, SIGKILL);
            break;
        } else if (c == SUSP) {
            raise_sig(tdev, lbuf, SIGSTOP);
        } else if (c == ERASE) {
            if (!rbuffer_erase(cooked))
                continue;
        } else if (c == KILL) {
            // TODO shrink the rbuffer
        } else {
            goto keep;
        }

        if ((_lf & _ECHOE) && c == ERASE) {
            rbuffer_put(output, '\x8'); 
            rbuffer_put(output, ' '); 
            rbuffer_put(output, '\x8'); 
        }
        if ((_lf & _ECHOK) && c == KILL) {
            rbuffer_put(output, c);
            rbuffer_put(output, '\n');
        }

        continue;

    keep:
        if ((_lf & _ECHO)) {
            rbuffer_put(output, c);
        }

        goto put_char;

    do_out:
        if (c == '\n' && (_of & _ONLCR)) {
            putc_safe(cooked, '\r');
        }

    put_char:
        if (!allow_more) {
            continue;
        }

        if (!out && (_lf & _IEXTEN) && lcntl_slave_put) {
            allow_more = lcntl_slave_put(tdev, lbuf, c);
        } else {
            allow_more = rbuffer_put_nof(cooked, c);
        }
    }

    if (!out && !rbuffer_empty(output) && !(_lf & _NOFLSH)) {
        term_flush(tdev);
    }

    line_flip(lbuf);

    return i;
}

int
lcntl_transform_inseq(struct term* tdev)
{
    return lcntl_transform_seq(tdev, &tdev->line_in, false);
}

int
lcntl_transform_outseq(struct term* tdev)
{
    return lcntl_transform_seq(tdev, &tdev->line_out, true);
}