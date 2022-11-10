#include <lunaix/buffer.h>
#include <lunaix/mm/valloc.h>

struct vecbuf*
vbuf_alloc(struct vecbuf** vec, void* buf, size_t size)
{
    struct vecbuf* vbuf = valloc(sizeof(struct vecbuf));
    struct vecbuf* _vec = *vec;

    *vbuf = (struct vecbuf){ .buf = { .buffer = buf, .size = size },
                             .acc_sz = vbuf_size(_vec) + size };

    if (_vec) {
        llist_append(&_vec->components, &vbuf->components);
    } else {
        llist_init_head(&vbuf->components);
        *vec = vbuf;
    }

    return vbuf;
}

void
vbuf_free(struct vecbuf* vbuf)
{
    struct vecbuf *pos, *n;
    llist_for_each(pos, n, &vbuf->components, components)
    {
        vfree(pos);
    }
    vfree(pos);
}