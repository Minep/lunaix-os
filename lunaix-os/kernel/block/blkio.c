#include <lunaix/blkio.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>

static struct cake_pile* blkio_reqpile;

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

    return ctx;
}

void
blkio_commit(struct blkio_context* ctx, struct blkio_req* req)
{
    req->flags |= BLKIO_PENDING;
    req->io_ctx = ctx;
    llist_append(&ctx->queue, &req->reqs);

    // if the pipeline is not running (e.g., stalling). Then we should schedule
    // one immediately and kick it started.
    if (!ctx->busy) {
        blkio_schedule(ctx);
    }
}

void
blkio_schedule(struct blkio_context* ctx)
{
    if (llist_empty(&ctx->queue)) {
        return;
    }

    struct blkio_req* head = (struct blkio_req*)ctx->queue.next;
    llist_delete(&head->reqs);

    head->flags |= BLKIO_BUSY;
    head->io_ctx->busy++;

    ctx->handle_one(head);
}

void
blkio_complete(struct blkio_req* req)
{
    req->flags &= ~(BLKIO_BUSY | BLKIO_PENDING);

    if (req->completed) {
        req->completed(req);
    }

    // FIXME Not working in first process! Need a dummy process.
    // Wake all blocked processes on completion,
    //  albeit should be no more than one process in everycase (by design)
    pwake_all(&req->wait);

    if ((req->flags & BLKIO_FOC)) {
        blkio_free_req(req);
    }

    req->io_ctx->busy--;
}