#ifndef __LUNAIX_BLKBUF_H
#define __LUNAIX_BLKBUF_H

#include <lunaix/blkio.h>
#include <lunaix/bcache.h>
#include <lunaix/block.h>

struct blkbuf_cache
{
    union {
        struct {
            unsigned int blksize;
        };
        struct bcache cached;
    };
    struct llist_header dirty;
    struct block_dev* blkdev;
};

struct blk_buf {
    void* raw;
    bcobj_t cobj;
    struct llist_header dirty;
    struct blkio_req* breq;
};

typedef void* bbuf_t;

#define BLOCK_BUFFER(type, name)    \
    union {                         \
        type* name;                 \
        bbuf_t bb_##name;             \
    }


struct blkbuf_cache*
blkbuf_create(struct block_dev* blkdev, unsigned int blk_size);

bbuf_t
blkbuf_take(struct blkbuf_cache* bc, unsigned int block_id);

static inline bbuf_t
blkbuf_refonce(bbuf_t buf)
{
    bcache_refonce(((struct blk_buf*)buf)->cobj);
    return buf;
}

static inline void*
blkbuf_data(bbuf_t buf) 
{
    return ((struct blk_buf*)buf)->raw;
}

static inline void
blkbuf_mark_dirty(bbuf_t buf)
{
    struct blk_buf* bbuf;
    struct blkbuf_cache* bc;
    
    bbuf = ((struct blk_buf*)buf);
    bc = bcache_holder_embed(bbuf->cobj, struct blkbuf_cache, cached);
    
    if (llist_empty(&bbuf->dirty)) {
        llist_append(&bc->dirty, &bbuf->dirty);
    }
}

void
blkbuf_sync(bbuf_t buf);

void
blkbuf_release(struct blkbuf_cache* bc);

void
blkbuf_put(bbuf_t buf);

#endif /* __LUNAIX_BLKBUF_H */
