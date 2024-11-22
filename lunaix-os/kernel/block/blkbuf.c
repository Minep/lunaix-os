#include <lunaix/blkbuf.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/owloysius.h>
#include <lunaix/syslog.h>
#include <asm/muldiv64.h>

LOG_MODULE("blkbuf")  

#define bb_cache_obj(bcache) \
            container_of(bcache, struct blkbuf_cache, cached)

#define to_blkbuf(bbuf) ((struct blk_buf*)(bbuf))

static bcache_zone_t bb_zone;
static struct cake_pile* bb_pile;

static inline u64_t
__tolba(struct blkbuf_cache* cache, unsigned int blk_id)
{
    return udiv64(((u64_t)cache->blksize * (u64_t)blk_id), 
                    cache->blkdev->blk_size);
}

static void
__blkbuf_do_sync(struct bcache* bc, unsigned long tag, void* data)
{
    return;
}

static void
__blkbuf_sync_callback(struct blkio_req* req)
{
    struct blk_buf* buf;

    buf = (struct blk_buf*)req->evt_args;

    if (req->errcode) {
        ERROR("sync failed: io error, 0x%x", req->errcode);
        return;
    }
}

static void
__blkbuf_evict_callback(struct blkio_req* req)
{
    struct blk_buf* buf;

    buf = (struct blk_buf*)req->evt_args;

    if (req->errcode) {
        ERROR("sync on evict failed (io error, 0x%x)", req->errcode);
    }
    
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
    req = buf->breq;

    if (llist_empty(&buf->dirty)) {
        __blkbuf_evict_callback(req);
        blkio_free_req(req);
        return;
    }
    
    // since we are evicting, don't care if the sync is failed
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
__blkbuf_take_slow_lockness(struct blkbuf_cache* bc, unsigned int block_id)
{
    struct blk_buf* buf;
    struct blkio_req* req;
    struct vecbuf* vbuf;
    void* data;
    u64_t lba;

    data = valloc(bc->blksize);

    vbuf = NULL;
    vbuf_alloc(&vbuf, data, bc->blksize);

    lba = __tolba(bc, block_id);
    buf = (struct blk_buf*)cake_grab(bb_pile);
    req = blkio_vreq(vbuf, lba, __blkbuf_sync_callback, buf, 0);

    // give dirty a know state
    llist_init_head(&buf->dirty);

    blkio_setread(req);
    blkio_bindctx(req, bc->blkdev->blkio);
    blkio_commit(req, BLKIO_WAIT);

    if (req->errcode) {
        ERROR("block io error (0x%x)", req->errcode);
        cake_release(bb_pile, buf);
        return (bbuf_t)INVL_BUFFER;
    }

    buf->raw = data;
    buf->cobj = bcache_put_and_ref(&bc->cached, block_id, buf);
    buf->breq = req;

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
    mutex_init(&bb_cache->lock);

    return bb_cache;
}

bbuf_t
blkbuf_take(struct blkbuf_cache* bc, unsigned int block_id)
{
    bcobj_t cobj;
    mutex_lock(&bc->lock);
    if (bcache_tryget(&bc->cached, block_id, &cobj)) {
        mutex_unlock(&bc->lock);
        return (bbuf_t)bcached_data(cobj);
    }

    bbuf_t buf = __blkbuf_take_slow_lockness(bc, block_id);
    
    mutex_unlock(&bc->lock);
    return buf;
}

void
blkbuf_put(bbuf_t buf)
{
    if (unlikely(!buf || blkbuf_errbuf(buf))) {
        return;
    }

    struct blk_buf* bbuf;
    bbuf = to_blkbuf(buf);

    bcache_return(bbuf->cobj);
}

void
blkbuf_dirty(bbuf_t buf)
{
    assert(buf && !blkbuf_errbuf(buf));

    struct blk_buf* bbuf;
    struct blkbuf_cache* bc;
    
    bbuf = ((struct blk_buf*)buf);
    bc = bcache_holder_embed(bbuf->cobj, struct blkbuf_cache, cached);
    
    mutex_lock(&bc->lock);
    
    if (llist_empty(&bbuf->dirty)) {
        llist_append(&bc->dirty, &bbuf->dirty);
    }

    mutex_unlock(&bc->lock);
}

static inline void
__schedule_sync_event(struct blk_buf* bbuf, bool wait)
{
    struct blkio_req* blkio;

    blkio = bbuf->breq;

    blkio_setwrite(blkio);
    blkio_commit(blkio, wait ? BLKIO_WAIT : BLKIO_NOWAIT);

    llist_delete(&bbuf->dirty);
}

void
blkbuf_schedule_sync(bbuf_t buf)
{
    struct blk_buf* bbuf;
    bbuf = to_blkbuf(buf);

    __schedule_sync_event(bbuf, false);
}

bool
blkbuf_syncall(struct blkbuf_cache* bc, bool async)
{
    struct blk_buf *pos, *n;

    mutex_lock(&bc->lock);

    llist_for_each(pos, n, &bc->dirty, dirty) {
        __schedule_sync_event(pos, !async);
    }

    mutex_unlock(&bc->lock);

    if (async) {
        return true;
    }

    return llist_empty(&bc->dirty);
}

void
blkbuf_release(struct blkbuf_cache* bc)
{
    bcache_free(&bc->cached);
    vfree(bc);
}

static void
__init_blkbuf()
{
    bb_zone = bcache_create_zone("blk_buf");
    bb_pile = cake_new_pile("blk_buf", sizeof(struct blk_buf), 1, 0);
}
owloysius_fetch_init(__init_blkbuf, on_sysconf)