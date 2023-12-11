#ifndef __LUNAIX_RBUFFER_H
#define __LUNAIX_RBUFFER_H

#include <lunaix/types.h>

struct rbuffer
{
    char* buffer;
    size_t maxsz;
    off_t ptr;
    size_t len;
};

static inline void
rbuffer_init(struct rbuffer* rb, char* buf, size_t maxsz)
{
    *rb = (struct rbuffer){ .buffer = buf, .maxsz = maxsz };
}

struct rbuffer*
rbuffer_create(char* buf, size_t maxsz);

int
rbuffer_erase(struct rbuffer* rb);

int
rbuffer_put(struct rbuffer* rb, char c);

int
rbuffer_puts(struct rbuffer* rb, char* c, size_t len);

int
rbuffer_gets(struct rbuffer* rb, char* buf, size_t len);

int
rbuffer_get(struct rbuffer* rb, char* c);


static inline void
rbuffer_clear(struct rbuffer* rb)
{
    rb->len = rb->ptr = 0;
}

static inline bool
rbuffer_empty(struct rbuffer* rb)
{
    return rb->len == 0;
}

static inline bool
rbuffer_full(struct rbuffer* rb)
{
    return rb->len == rb->maxsz;
}

static inline int 
rbuffer_put_nof(struct rbuffer* rb, char c)
{
    if (rbuffer_full(rb)) {
        return 0;
    }

    return rbuffer_put(rb, c);
}

#endif /* __LUNAIX_RBUFFER_H */
