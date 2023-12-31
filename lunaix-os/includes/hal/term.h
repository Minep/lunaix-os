#ifndef __LUNAIX_TERM_H
#define __LUNAIX_TERM_H

#include <lunaix/device.h>
#include <lunaix/ds/rbuffer.h>
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
#define LSTATE_EOL (1)
#define LSTATE_EOF (1 << 1)
#define LSTATE_SIGRAISE (1 << 2)

typedef struct rbuffer** lbuf_ref_t;
#define ref_current(lbuf) (&(lbuf)->current)
#define ref_next(lbuf) (&(lbuf)->next)
#define deref(bref) (*(bref))

struct term_lcntl
{
    struct term* term;
    int (*process_and_put)(struct term*, struct linebuffer*, char);
};

/**
 * @brief Communication port capability that a device is supported natively, 
 *          or able to emulate low level serial transmission behaviour specify 
 *          by POSIX1-2008, section 11.
 * 
 */
#define TERMPORT_CAP 0x4d524554U

/**
 * @brief A termios capability that a device provide interfaces which is 
 *          compliant to POSIX1-2008
 * 
 */
#define TERMIOS_CAP 0x534f4954U

struct termport_capability
{
    CAPABILITY_META;

    void (*set_speed)(struct device*, speed_t);
    void (*set_cntrl_mode)(struct device*, tcflag_t);
};

struct term
{
    struct device* dev;
    struct device* chdev;
    struct term_lcntl* lcntl;
    struct linebuffer line_out;
    struct linebuffer line_in;
    char* scratch_pad;
    pid_t fggrp;

    struct termport_capability* tp_cap;

    /* -- POSIX.1-2008 compliant fields -- */
    tcflag_t iflags;
    tcflag_t oflags;
    tcflag_t lflags;
    tcflag_t cflags;
    cc_t cc[_NCCS];
    speed_t iospeed;
};

extern struct device* sysconsole;

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

#endif /* __LUNAIX_TERM_H */
