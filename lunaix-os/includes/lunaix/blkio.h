#ifndef __LUNAIX_BLKIO_H
#define __LUNAIX_BLKIO_H

#include <lunaix/buffer.h>
#include <lunaix/ds/llist.h>
#include <lunaix/ds/waitq.h>
#include <lunaix/types.h>

#define BLKIO_WRITE 0x1
#define BLKIO_ERROR 0x2

#define BLKIO_BUSY 0x4
#define BLKIO_PENDING 0x8

#define BLKIO_WAIT 0x1

// Free on complete
#define BLKIO_FOC 0x10

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
};

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

void
blkio_free_req(struct blkio_req* req);

/**
 * @brief Commit an IO request to scheduler.
 *
 * @param ctx
 * @param req
 */
void
blkio_commit(struct blkio_context* ctx, struct blkio_req* req, int options);

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
