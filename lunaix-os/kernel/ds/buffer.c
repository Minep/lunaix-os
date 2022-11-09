#include <lunaix/buffer.h>
#include <lunaix/mm/valloc.h>

struct vecbuf*
vbuf_alloc(struct vecbuf* vec, void* buf, size_t size)
{
    struct vecbuf* vbuf = valloc(sizeof(struct vecbuf));

    *vbuf = (struct vecbuf){ .buf = { .buffer = buf, .size = size },
                             .acc_sz = vbuf_size(vec) + size };

    if (vec) {
        llist_append(&vec->components, &vbuf->components);
    } else {
        llist_init_head(&vbuf->components);
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