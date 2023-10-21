#ifndef __LUNAIX_TERM_H
#define __LUNAIX_TERM_H

#include <lunaix/device.h>

struct term;

struct term_lcntl
{
    struct llist_header lcntls;
    size_t (*apply)(struct term* termdev, char* line, size_t len);
};

struct linebuffer
{
    char* current;
    char* next;
    size_t sz_hlf;
    off_t ptr;
};

struct term
{
    struct device* dev;
    struct device* chdev;
    struct llist_header lcntl_stack;
    struct linebuffer line;
    pid_t fggrp;
};

struct term*
term_create();

int
term_bind(struct term* tdev, struct device* chdev);

int
term_push_lcntl(struct term* tdev, struct term_lcntl* lcntl);

int
term_pop_lcntl(struct term* tdev);

struct term_lcntl*
term_get_lcntl(u32_t lcntl_index);

void
line_flip(struct linebuffer* lbf);

void
line_alloc(struct linebuffer* lbf, size_t sz_hlf);

void
line_free(struct linebuffer* lbf, size_t sz_hlf);

static inline int
line_put_next(struct linebuffer* lbf, char val, int delta)
{
    size_t dptr = (size_t)(lbf->ptr + delta);
    if (dptr >= lbf->sz_hlf) {
        return 0;
    }

    lbf->next[dptr] = val;
    lbf->ptr++;
    return 1;
}

int
term_sendline(struct term* tdev, size_t len);

int
term_readline(struct term* tdev, size_t len);

#endif /* __LUNAIX_TERM_H */
