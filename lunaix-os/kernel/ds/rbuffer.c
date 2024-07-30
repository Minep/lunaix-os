#include <lunaix/ds/rbuffer.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

#include <klibc/string.h>

struct rbuffer*
rbuffer_create(char* buf, size_t maxsz)
{
    struct rbuffer* rb = valloc(sizeof(struct rbuffer));
    rbuffer_init(rb, buf, maxsz);

    return rb;
}

int
rbuffer_erase(struct rbuffer* rb)
{
    if (rb->len == 0) {
        return 0;
    }
    rb->ptr = (rb->ptr - 1) % rb->maxsz;
    rb->len--;

    return 1;
}

int
rbuffer_put(struct rbuffer* rb, char c)
{
    rb->buffer[rb->ptr] = c;
    rb->ptr = (rb->ptr + 1) % rb->maxsz;
    rb->len = MIN(rb->len + 1, rb->maxsz);

    return rb->len < rb->maxsz;
}

int
rbuffer_puts(struct rbuffer* rb, char* buf, size_t len)
{
    if (!len)
        return 0;

    size_t nlen = MIN(len, rb->maxsz);
    size_t ptr = (rb->ptr + nlen) % rb->maxsz;
    size_t ptr_start = rb->ptr;

    buf = &buf[nlen];
    if (ptr_start >= ptr) {
        size_t llen = rb->maxsz - ptr_start;
        memcpy(&rb->buffer[ptr_start], &buf[-nlen], llen);
        memcpy(&rb->buffer[0], &buf[-nlen + llen], ptr);
    } else {
        memcpy(&rb->buffer[ptr_start], &buf[-nlen], nlen);
    }

    rb->ptr = ptr;
    rb->len = MIN(rb->len + nlen, rb->maxsz);

    return nlen;
}

int
rbuffer_gets_opt(struct rbuffer* rb, char* buf, size_t len, bool consumed)
{
    if (!len || !rb->len)
        return 0;

    size_t nlen = MIN(len, rb->len);
    size_t ptr_start = (rb->ptr - rb->len) % rb->maxsz;
    size_t ptr_end = (ptr_start + nlen) % rb->maxsz;

    buf = &buf[nlen];
    if (ptr_start >= ptr_end) {
        size_t llen = rb->maxsz - ptr_start;
        memcpy(&buf[-nlen], &rb->buffer[ptr_start], llen);
        memcpy(&buf[-nlen + llen], &rb->buffer[0], ptr_end);
    } else {
        memcpy(&buf[-nlen], &rb->buffer[ptr_start], nlen);
    }

    if (consumed) {
        rb->len -= nlen;
    }

    return nlen;
}

int
rbuffer_get(struct rbuffer* rb, char* c)
{
    if (rb->len == 0) {
        return 0;
    }

    size_t ptr_start = (rb->ptr - rb->len) % rb->maxsz;
    rb->len--;

    *c = rb->buffer[ptr_start];

    return 1;
}