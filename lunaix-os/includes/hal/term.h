#ifndef __LUNAIX_TERM_H
#define __LUNAIX_TERM_H

#include <lunaix/device.h>
#include <lunaix/ds/rbuffer.h>
#include <lunaix/signal_defs.h>

#include <usr/lunaix/term.h>

struct term;

struct linebuffer
{
    struct rbuffer *next;
    struct rbuffer *current;
    short sflags;
    short sz_hlf;
};
#define LSTATE_EOL (1)
#define LSTATE_EOF (1 << 1)
#define LSTATE_SIGRAISE (1 << 2)

typedef struct rbuffer** lbuf_ref_t;
#define ref_current(lbuf) (&(lbuf)->current)
#define ref_next(lbuf) (&(lbuf)->next)
#define deref(bref) (*(bref))

struct term_lcntl
{
    struct llist_header lcntls;
    struct term* term;
    size_t (*process_and_put)(struct term*, struct linebuffer*, char);
};

struct term
{
    struct device* dev;
    struct device* chdev;
    struct llist_header lcntl_stack;
    struct linebuffer line_out;
    struct linebuffer line_in;
    pid_t fggrp;

    struct
    {
        int (*set_speed)(struct device*, speed_t);
    } chdev_ops;

    /* -- POSIX.1-2008 compliant fields -- */
    tcflag_t iflags;
    tcflag_t oflags;
    tcflag_t lflags;
    cc_t cc[_NCCS];
    speed_t iospeed;
};

struct term*
term_create(struct device* chardev, char* suffix);

int
term_bind(struct term* tdev, struct device* chdev);

int
term_push_lcntl(struct term* tdev, struct term_lcntl* lcntl);

int
term_pop_lcntl(struct term* tdev);

struct term_lcntl*
term_get_lcntl(u32_t lcntl_index);

static inline void
line_flip(struct linebuffer* lbf) {
    struct rbuffer* tmp = lbf->current;
    lbf->current = lbf->next;
    lbf->next = tmp;
}

void
line_alloc(struct linebuffer* lbf, size_t sz_hlf);

void
line_free(struct linebuffer* lbf, size_t sz_hlf);

void
term_sendsig(struct term* tdev, int signal);

int
term_flush(struct term* tdev);

int
term_read(struct term* tdev);

int
lcntl_transform_inseq(struct term* tdev);

int
lcntl_transform_outseq(struct term* tdev);

#endif /* __LUNAIX_TERM_H */
