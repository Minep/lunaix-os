#ifndef __LUNAIX_BUFFER_H
#define __LUNAIX_BUFFER_H

#include <lunaix/ds/llist.h>
#include <lunaix/types.h>

struct membuf
{
    void* buffer;
    size_t size;
};

struct vecbuf
{
    struct llist_header components;
    struct membuf buf;
    size_t acc_sz;
};

/**
 * @brief Free a vectorized buffer
 *
 * @param vbuf
 */
void
vbuf_free(struct vecbuf* vbuf);

/**
 * @brief Allocate a buffer, or append to `vec` (if not NULL) as a component
 * thus to form a vectorized buffer.
 *
 * @param vec the buffer used to construct a vectorized buffer.
 * @param buf a memeory region that holds data or partial data if vectorized
 * @param len maximum number of bytes should recieved.
 * @return struct vecbuf*
 */
struct vecbuf*
vbuf_alloc(struct vecbuf** vec, void* buf, size_t len);

static inline size_t
vbuf_size(struct vecbuf* vbuf)
{
    if (!vbuf) {
        return 0;
    }

    struct vecbuf* last =
      list_entry(vbuf->components.prev, struct vecbuf, components);
    return last->acc_sz;
}

#endif /* __LUNAIX_BUFFER_H */
