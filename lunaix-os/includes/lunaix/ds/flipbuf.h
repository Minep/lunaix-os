#ifndef __LUNAIX_FLIPBUF_H
#define __LUNAIX_FLIPBUF_H

#include <lunaix/types.h>
#include <lunaix/spike.h>

struct flipbuf
{
    void* buf;
    void* top;
    unsigned long half_size;
};

#define DEFINE_FLIPBUF(name, halfsz, buf_) \
            struct flipbuf name = { .buf=buf_, .top=buf_, .half_size=halfsz }

static inline void*
flipbuf_under(struct flipbuf* fbuf)
{
    ptr_t off;

    off  = __ptr(fbuf->top);
    off += fbuf->half_size;
    off %= fbuf->half_size;
    off += __ptr(fbuf->buf);
    
    return (void*)off;
}

static inline void*
flipbuf_top(struct flipbuf* fbuf)
{
    return fbuf->top;
}

static inline void*
flipbuf_flip(struct flipbuf* fbuf)
{
    fbuf->top = flipbuf_under(fbuf);
    return fbuf->top;
}

#endif /* __LUNAIX_FLIPBUF_H */
