#ifndef __LUNAIX_BLKIO_H
#define __LUNAIX_BLKIO_H

#include <lunaix/buffer.h>
#include <lunaix/buffer.h>
#include <lunaix/ds/llist.h>
#include <lunaix/ds/waitq.h>
#include <lunaix/ds/mutex.h>
#include <lunaix/types.h>

#define BLKIO_WRITE 0x1
#define BLKIO_ERROR 0x2

#define BLKIO_BUSY 0x4
#define BLKIO_PENDING 0x8
// Free on complete
#define BLKIO_FOC 0x10
#define BLKIO_SHOULD_WAIT 0x20

#define BLKIO_WAIT 0x1
#define BLKIO_NOWAIT 0
#define BLKIO_NOASYNC 0x2

#define BLKIO_SCHED_IDEL 0x1

struct blkio_req;

typedef void (*blkio_cb)(struct blkio_req*);
typedef void (*req_handler)(struct blkio_req*);

struct blkio_req
{
    struct llist_header reqs;
    struct blkio_context* io_ctx;
    struct vecbuf* vbuf;
    u32_t flags;
    waitq_t wait;
    u64_t blk_addr;
    void* evt_args;
    blkio_cb completed;
    int errcode;
};

struct blkio_context
{
    struct llist_header queue;

    struct
    {
        u32_t seektime;
        u32_t rotdelay;
    } metrics;

    req_handler handle_one;
    u32_t state;
    u32_t busy;
    void* driver;

    mutex_t lock;
};

static inline void
blkio_lock(struct blkio_context* contex)
{
    mutex_lock(&contex->lock);
}

static inline void
blkio_unlock(struct blkio_context* contex)
{
    mutex_unlock(&contex->lock);
}

static inline bool
blkio_stalled(struct blkio_context* contex)
{
    return !contex->busy;
}

void
blkio_init();

/**
 * @brief Vectorized read request
 *
 * @param vbuf
 * @param start_lba
 * @param completed
 * @param evt_args
 * @param options
 * @return struct blkio_req*
 */
struct blkio_req*
blkio_vrd(struct vecbuf* vbuf,
          u64_t start_lba,
          blkio_cb completed,
          void* evt_args,
          u32_t options);

/**
 * @brief Vectorized write request
 *
 * @param vbuf
 * @param start_lba
 * @param completed
 * @param evt_args
 * @param options
 * @return struct blkio_req*
 */
struct blkio_req*
blkio_vwr(struct vecbuf* vbuf,
          u64_t start_lba,
          blkio_cb completed,
          void* evt_args,
          u32_t options);

/**
 * @brief Vectorized request (no write/read preference)
 *
 * @param vbuf
 * @param start_lba
 * @param completed
 * @param evt_args
 * @param options
 * @return struct blkio_req*
 */
static inline struct blkio_req*
blkio_vreq(struct vecbuf* buffer,
          u64_t start_lba,
          blkio_cb completed,
          void* evt_args,
          u32_t options) {
    /*
        This is currently aliased to blkio_vrd. Although `no preference`
        does essentially mean `default read`, the blkio_vreq just used
        to enhance readability
    */
    return blkio_vrd(buffer, start_lba, completed, evt_args, options);
}


/**
 * @brief Bind a block IO context to request
 *
 * @param ctx
 * @param req
 */
static inline void
blkio_bindctx(struct blkio_req* req, struct blkio_context* ctx)
{
    req->io_ctx = ctx;
}

/**
 * @brief Set block IO request to read
 *
 * @param ctx
 * @param req
 */
static inline void
blkio_setread(struct blkio_req* req)
{
    if ((req->flags & BLKIO_PENDING)) {
        return;
    }
    
    req->flags &= ~BLKIO_WRITE;
}

/**
 * @brief Set block IO request to write
 *
 * @param ctx
 * @param req
 */
static inline void
blkio_setwrite(struct blkio_req* req)
{
    if ((req->flags & BLKIO_PENDING)) {
        return;
    }

    req->flags |= BLKIO_WRITE;
}

/**
 * @brief Set callback when request complete
 * 
 * @param req 
 * @param on_completed 
 */
static inline void
blkio_when_completed(struct blkio_req* req, blkio_cb on_completed)
{
    req->completed = on_completed;
}

static inline bool
blkio_is_pending(struct blkio_req* req)
{
    return (req->flags & BLKIO_PENDING);
}

/**
 * @brief Mark request to be freed-on-completion (FOC)
 * 
 * @param req 
 */
static inline void
blkio_mark_foc(struct blkio_req* req)
{
    req->flags |= BLKIO_FOC;
}

/**
 * @brief Mark request to be not-freed-on-completion (nFOC)
 * 
 * @param req 
 */
static inline void
blkio_mark_nfoc(struct blkio_req* req)
{
    req->flags &= ~BLKIO_FOC;
}

int
blkio_read_aligned(struct blkio_context* ctx, 
                   unsigned long lba, void* block, size_t n_blk);

int
blkio_read(struct blkio_context* ctx, 
           unsigned long offset, void* block, size_t len);

int
blkio_write_aligned(struct blkio_context* ctx, 
                    unsigned long lba, void* block, size_t n_blk);

int
blkio_read(struct blkio_context* ctx, 
           unsigned long offset, void* block, size_t len);

void
blkio_free_req(struct blkio_req* req);

/**
 * @brief Commit an IO request to scheduler.
 *
 * @param ctx
 * @param req
 */
void
blkio_commit(struct blkio_req* req, int options);


/**
 * @brief Schedule an IO request to be handled.
 *
 * @param ctx
 */
void
blkio_schedule(struct blkio_context* ctx);

/**
 * @brief Notify the scheduler when request is completed, either successful or
 * failed.
 *
 * @param ctx
 * @param req
 */
void
blkio_complete(struct blkio_req* req);

/**
 * @brief Create a new block IO scheduling context
 *
 * @param handler Handler to handle request
 * @return struct blkio_context*
 */
struct blkio_context*
blkio_newctx(req_handler handler);

#endif /* __LUNAIX_BLKIO_H */
