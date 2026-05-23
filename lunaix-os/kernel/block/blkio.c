#include <lunaix/blkio.h>
#include <lunaix/syslog.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>

#include <asm/cpu.h>

static struct cake_pile* blkio_reqpile;

LOG_MODULE("blkio")

void
blkio_init()
{
    blkio_reqpile = cake_new_pile("blkio_req", sizeof(struct blkio_req), 1, 0);
}

static inline struct blkio_req*
__blkio_req_create(struct vecbuf* buffer,
                   u64_t start_lba,
                   blkio_cb completed,
                   void* evt_args,
                   u32_t options)
{
    options = options & ~0xf;
    struct blkio_req* breq = (struct blkio_req*)cake_grab(blkio_reqpile);
    *breq = (struct blkio_req){ .blk_addr = start_lba,
                                .completed = completed,
                                .flags = options,
                                .evt_args = evt_args };
    breq->vbuf = buffer;
    waitq_init(&breq->wait);
    return breq;
}

struct blkio_req*
blkio_vrd(struct vecbuf* buffer,
          u64_t start_lba,
          blkio_cb completed,
          void* evt_args,
          u32_t options)
{
    return __blkio_req_create(buffer, start_lba, completed, evt_args, options);
}

struct blkio_req*
blkio_vwr(struct vecbuf* buffer,
          u64_t start_lba,
          blkio_cb completed,
          void* evt_args,
          u32_t options)
{
    struct blkio_req* breq =
      __blkio_req_create(buffer, start_lba, completed, evt_args, options);
    breq->flags |= BLKIO_WRITE;
    return breq;
}

void
blkio_free_req(struct blkio_req* req)
{
    cake_release(blkio_reqpile, (void*)req);
}

struct blkio_context*
blkio_newctx(req_handler handler)
{
    struct blkio_context* ctx =
      (struct blkio_context*)vzalloc(sizeof(struct blkio_context));
    ctx->handle_one = handler;

    llist_init_head(&ctx->queue);
    mutex_init(&ctx->lock);

    return ctx;
}

void
blkio_commit(struct blkio_req* req, int options)
{
    struct blkio_context* ctx;

    if (blkio_is_pending(req)) {
        // prevent double submition
        return;
    }

    assert(req->io_ctx);

    req->flags |= BLKIO_PENDING;
    
    if ((options & BLKIO_WAIT)) {
        req->flags |= BLKIO_SHOULD_WAIT;
        prepare_to_wait(&req->wait);
    }
    else {
        req->flags &= ~BLKIO_SHOULD_WAIT;
    }

    ctx = req->io_ctx;

    blkio_lock(ctx);
    
    llist_append(&ctx->queue, &req->reqs);
    
    blkio_unlock(ctx);

    /*
     * By design, the blkio will schedule the next eligible item 
     * from the pipeline right after the completion of current
     * item (it will do so in the completion context of the item, 
     * e.g., inside interrupt handler)
     *
     * A pipeline is stalled if such scheduling is failed (pipeline
     * is empty). This happened when the request comes in low 
     * frequence. In that case, a explicit call to blkio_schedule is
     * required to get the pipeline tick again.
     */

    if (blkio_stalled(ctx)) {
        if ((options & BLKIO_WAIT)) {
            blkio_schedule(ctx);
            try_wait_check_stall();
            return;
        }
        blkio_schedule(ctx);
    } else if ((options & BLKIO_WAIT)) {
        try_wait_check_stall();
    }
}

void
blkio_schedule(struct blkio_context* ctx)
{
    // stall the pipeline if ctx is locked by others.
    // we must not try to hold the lock in this case, as
    //  blkio_schedule will be in irq context most of the
    //  time, we can't afford the waiting there.
    if (mutex_on_hold(&ctx->lock)) {
        return;
    }

    // will always successed when in irq context
    blkio_lock(ctx);

    if (llist_empty(&ctx->queue)) {
        blkio_unlock(ctx);
        return;
    }

    struct blkio_req* head = (struct blkio_req*)ctx->queue.next;
    llist_delete(&head->reqs);

    head->flags |= BLKIO_BUSY;
    ctx->busy++;

    blkio_unlock(ctx);

    ctx->handle_one(head);
}

void
blkio_complete(struct blkio_req* req)
{
    struct blkio_context* ctx;

    ctx = req->io_ctx;
    req->flags &= ~(BLKIO_BUSY | BLKIO_PENDING);

    // Wake all blocked processes on completion,
    //  albeit should be no more than one process in everycase (by design)
    if ((req->flags & BLKIO_SHOULD_WAIT)) {
        assert(!waitq_empty(&req->wait));
        pwake_all(&req->wait);
    }

    if (req->errcode) {
        WARN("request completed with error. (errno=0x%x, ctx=%p)",
                req->errcode, (ptr_t)ctx);
    }

    if (req->completed) {
        req->completed(req);
    }

    if ((req->flags & BLKIO_FOC)) {
        blkio_free_req(req);
    }

    ctx->busy--;
}
