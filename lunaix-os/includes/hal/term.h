#ifndef __LUNAIX_TERM_H
#define __LUNAIX_TERM_H

#include <lunaix/device.h>
#include <lunaix/ds/rbuffer.h>
#include <lunaix/ds/waitq.h>
#include <lunaix/signal_defs.h>

#include <usr/lunaix/term.h>

struct term;

struct linebuffer
{
    struct rbuffer* next;
    struct rbuffer* current;
    short sflags;
    short sz_hlf;
};
#define LEVT_EOL (1)
#define LEVT_EOF (1 << 1)
#define LEVT_SIGRAISE (1 << 2)

typedef struct rbuffer** lbuf_ref_t;
#define ref_current(lbuf) (&(lbuf)->current)
#define ref_next(lbuf) (&(lbuf)->next)
#define deref(bref) (*(bref))

struct term;

struct termport_pot_ops
{
    void (*set_speed)(struct device*, speed_t);
    void (*set_clkbase)(struct device*, unsigned int);
    void (*set_cntrl_mode)(struct device*, tcflag_t);
};

struct termport_potens
{
    POTENS_META;
    struct termport_pot_ops* ops;
    struct term* term;
};

struct term
{
    struct device* dev;
    struct device* chdev;
    struct linebuffer line_out;
    struct linebuffer line_in;
    char* scratch_pad;
    pid_t fggrp;

    struct termport_potens* tp_cap;
    waitq_t line_in_event;

    /* -- POSIX.1-2008 compliant fields -- */
    tcflag_t iflags;
    tcflag_t oflags;
    tcflag_t lflags;
    tcflag_t cflags;
    cc_t cc[_NCCS];

    /* -- END POSIX.1-2008 compliant fields -- */
    speed_t iospeed;
    speed_t clkbase;
    tcflag_t tflags;    // temp flags
};

extern struct device* sysconsole;

struct termport_potens*
term_attach_potens(struct device* chardev, 
                   struct termport_pot_ops* ops, char* suffix);

int
term_bind(struct term* tdev, struct device* chdev);

static inline void
line_flip(struct linebuffer* lbf)
{
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

void
term_notify_data_avaliable(struct termport_potens* cap);

#endif /* __LUNAIX_TERM_H */
