#include <lunaix/blkbuf.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/owloysius.h>

#define bb_cache_obj(bcache) \
            container_of(bcache, struct blkbuf_cache, cached)

#define to_blkbuf(bbuf) ((struct blk_buf*)(bbuf))

static bcache_zone_t bb_zone;
static struct cake_pile* bb_pile;

static inline unsigned int
__tolba(struct blkbuf_cache* cache, unsigned int blk_id)
{
    return (cache->blksize * blk_id) / cache->blkdev->blk_size;
}

static void
__blkbuf_do_sync(struct bcache* bc, unsigned long tag, void* data)
{
    return;
}

static void
__blkbuf_evict_callback(struct blkio_req* req)
{
    struct blk_buf* buf;

    buf = (struct blk_buf*)req->evt_args;
    
    vfree(buf->raw);
    vbuf_free(req->vbuf);
    cake_release(bb_pile, buf);
}

static void
__blkbuf_do_try_release(struct bcache* bc, void* data)
{
    struct blkio_req* req;
    struct blk_buf* buf;

    buf = (struct blk_buf*)data;
    req = &buf->breq;

    if (llist_empty(&buf->dirty)) {
        __blkbuf_evict_callback(req);
        blkio_free_req(req);
        return;
    }
    
    llist_delete(&buf->dirty);

    blkio_when_completed(req, __blkbuf_evict_callback);
    blkio_mark_foc(req);
    blkio_commit(req, 0);
}

static struct bcache_ops cache_ops = {
    .release_on_evict = __blkbuf_do_try_release,
    .sync_cached = __blkbuf_do_sync
};

static bbuf_t
__blkbuf_take_slow(struct blkbuf_cache* bc, unsigned int block_id)
{
    struct blk_buf* buf;
    struct blk_req* req;
    struct vecbuf* vbuf;
    void* data;

    data = valloc(bc->blksize);

    vbuf = vbuf_alloc(NULL, data, bc->blksize);
    req = blkio_vreq(vbuf, __tolba(bc, block_id), NULL, buf, 0);

    buf = (struct blk_buf*)cake_grab(bb_pile);
    buf->raw = data;
    buf->cobj = bcache_put_and_ref(&bc->cached, block_id, buf);
    buf->breq = req;

    blkio_setread(req);
    blkio_bindctx(req, bc->blkdev->blkio);
    blkio_commit(req, BLKIO_WAIT);

    return buf;
}

struct blkbuf_cache*
blkbuf_create(struct block_dev* blkdev, unsigned int blk_size)
{
    struct blkbuf_cache* bb_cache;

    assert(is_pot(blk_size));

    bb_cache = valloc(sizeof(*bb_cache));
    bb_cache->blkdev = blkdev;

    bcache_init_zone(&bb_cache->cached, bb_zone, 3, -1, blk_size, &cache_ops);
    llist_init_head(&bb_cache->dirty);
}

bbuf_t
blkbuf_take(struct blkbuf_cache* bc, unsigned int block_id)
{
    bcobj_t cobj;
    if (bcache_tryget(&bc->cached, block_id, &cobj)) {
        return (bbuf_t)bcached_data(cobj);
    }

    return __blkbuf_take_slow(bc, block_id);
}

void
blkbuf_put(bbuf_t buf)
{
    struct blk_buf* bbuf;
    bbuf = to_blkbuf(buf);

    bcache_return(bbuf->cobj);
}

void
blkbuf_sync(bbuf_t buf)
{
    struct blk_buf* bbuf;
    bbuf = to_blkbuf(buf);

    blkio_setwrite(bbuf->breq);
    blkio_commit(bbuf->breq, BLKIO_WAIT);
}

void
blkbuf_sync_all(struct blkbuf_cache* bc)
{
    bcache_flush(&bc->cached);

    assert(llist_empty(&bc->dirty));
    vfree(bc);
}

static void
__init_blkbuf()
{
    bb_zone = bcache_create_zone("blk_buf");
    bb_pile = cake_new_pile("blk_buf", sizeof(struct blk_buf), 1, 0);
}
owloysius_fetch_init(__init_blkbuf, on_earlyboot)