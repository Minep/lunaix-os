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

#define INVL_BUFFER      0xdeadc0de;

static inline bool
blkbuf_errbuf(bbuf_t buf) {
    return (ptr_t)buf == INVL_BUFFER;
}

static inline unsigned int
blkbuf_id(bbuf_t buf) 
{
    return to_bcache_node(((struct blk_buf*)buf)->cobj)->tag;
}

static inline unsigned int
blkbuf_refcounts(bbuf_t buf) 
{
    return to_bcache_node(((struct blk_buf*)buf)->cobj)->refs;
}

static inline bool
blkbuf_not_shared(bbuf_t buf)
{
    return blkbuf_refcounts(buf) == 1;
}


struct blkbuf_cache*
blkbuf_create(struct block_dev* blkdev, unsigned int blk_size);

bbuf_t
blkbuf_take(struct blkbuf_cache* bc, unsigned int block_id);

static inline bbuf_t
blkbuf_refonce(bbuf_t buf)
{
    if (likely(!blkbuf_errbuf(buf))) {
        bcache_refonce(((struct blk_buf*)buf)->cobj);
    }

    return buf;
}

static inline void*
blkbuf_data(bbuf_t buf) 
{
    assert(!blkbuf_errbuf(buf));
    return ((struct blk_buf*)buf)->raw;
}
#define block_buffer(buf, type) \
    ((type*)blkbuf_data(buf))

void
blkbuf_dirty(bbuf_t buf);

void
blkbuf_schedule_sync(bbuf_t buf);

void
blkbuf_release(struct blkbuf_cache* bc);

void
blkbuf_put(bbuf_t buf);

#endif /* __LUNAIX_BLKBUF_H */
