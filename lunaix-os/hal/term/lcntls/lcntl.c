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
    term_sendsig(at_term, SIGINT);
    lbuf->sflags |= LSTATE_SIGRAISE;
}

static inline int
lcntl_invoke_slaves(struct term* tdev, struct linebuffer* lbuf, char c)
{
    int allow_more = 0;
    struct term_lcntl *lcntl, *n;
    llist_for_each(lcntl, n, &tdev->lcntl_stack, lcntls)
    {
        allow_more = lcntl->process_and_put(tdev, lbuf, c);
        if (!allow_more) {
            break;
        }

        line_flip(lbuf);
    }

    return allow_more;
}

static inline int optimize("ipa-cp-clone")
lcntl_transform_seq(struct term* tdev, struct linebuffer* lbuf, bool out)
{
    struct rbuffer* raw = lbuf->current;
    struct rbuffer* cooked = lbuf->next;
    struct rbuffer* output = tdev->line_out.current;

    int i = 0, _if = tdev->iflags & -!out, _of = tdev->oflags & -!!out,
        _lf = tdev->lflags;
    int allow_more = 1, latest_eol = cooked->ptr;
    char c;
    bool should_flush = false;

#define EOL tdev->cc[_VEOL]
#define EOF tdev->cc[_VEOF]
#define ERASE tdev->cc[_VERASE]
#define KILL tdev->cc[_VKILL]
#define INTR tdev->cc[_VINTR]
#define QUIT tdev->cc[_VQUIT]
#define SUSP tdev->cc[_VSUSP]

    if (!out) {
        // Keep all cc's (except VMIN & VTIME) up to L2 cache.
        for (size_t i = 0; i < _VMIN; i++) {
            prefetch_rd(&tdev->cc[i], 2);
        }
    }

    while (rbuffer_get(raw, &c) && allow_more) {

        if (c == '\r' && ((_if & _ICRNL) || (_of & _OCRNL))) {
            c = '\n';
        } else if (c == '\n') {
            if ((_if & _INLCR) || (_of & (_ONLRET | _ONLCR))) {
                c = '\r';
            }
        }

        if (c == '\0') {
            if ((_if & _BRKINT)) {
                raise_sig(tdev, lbuf, SIGINT);
                break;
            }

            if ((_if & _IGNBRK)) {
                continue;
            }
        }

        if ('a' <= c && c <= 'z') {
            if ((_if & _IUCLC)) {
                c = c | 0b100000;
            } else if ((_of & _OLCUC)) {
                c = c & ~0b100000;
            }
        }

        if (c == '\n') {
            latest_eol = cooked->ptr + 1;
            if (!out && (_lf & _ECHONL)) {
                rbuffer_put(output, c);
            }
            should_flush = true;
        }

        if (out) {
            goto put_char;
        }

        // For input procesing

        if (c == '\n' || c == EOL) {
            lbuf->sflags |= LSTATE_EOL;
            goto keep;
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
            rbuffer_erase(cooked);
        } else {
            goto keep;
        }

        continue;

    keep:
        if ((_lf & _ECHO)) {
            rbuffer_put(output, c);
        }
        if ((_lf & _ECHOE) && c == ERASE) {
            rbuffer_erase(output);
        }
        if ((_lf & _ECHOK) && c == KILL) {
            rbuffer_put(output, c);
            rbuffer_put(output, '\n');
        }

    put_char:
        allow_more = rbuffer_put(cooked, c);
    }

    if (out || (_lf & _IEXTEN)) {
        line_flip(lbuf);
        lcntl_invoke_slaves(tdev, lbuf, c);
    }

    if (should_flush && !(_lf & _NOFLSH)) {
        term_flush(tdev);
    }

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
    return lcntl_transform_seq(tdev, &tdev->line_in, true);
}