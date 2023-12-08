#include <hal/term.h>

#include <lunaix/clock.h>
#include <lunaix/sched.h>

#include <usr/lunaix/term.h>

#define ONBREAK (LSTATE_EOF | LSTATE_SIGRAISE)
#define ONSTOP (LSTATE_SIGRAISE | LSTATE_EOL | LSTATE_EOF)

static int
do_read_raw(struct term* tdev)
{
    struct device* chdev = tdev->chdev;

    struct linebuffer* line_in = &tdev->line_in;
    size_t max_lb_sz = line_in->sz_hlf;

    line_flip(line_in);

    char* inbuffer = line_in->current->buffer;
    size_t min = tdev->cc[_VMIN] - 1;
    size_t sz = chdev->ops.read_async(chdev, inbuffer, 0, max_lb_sz);
    time_t t = clock_systime(), dt = 0;
    time_t expr = (tdev->cc[_VTIME] * 100) - 1;

    while (sz <= min && dt <= expr) {
        // XXX should we held the device lock while we are waiting?
        sched_yieldk();
        dt = clock_systime() - t;
        t += dt;

        max_lb_sz -= sz;

        // TODO pass a flags to read to indicate it is non blocking ops
        sz += chdev->ops.read_async(chdev, inbuffer, sz, max_lb_sz);
    }

    rbuffer_puts(line_in->next, inbuffer, sz);
    line_flip(line_in);

    return 0;
}

static int
do_read_raw_canno(struct term* tdev)
{
    struct device* chdev = tdev->chdev;
    struct linebuffer* line_in = &tdev->line_in;
    struct rbuffer* current_buf = line_in->current;
    int sz = chdev->ops.read(chdev, current_buf->buffer, 0, line_in->sz_hlf);

    current_buf->ptr = sz;
    current_buf->len = sz;

    return sz;
}

static int
term_read_noncano(struct term* tdev)
{
    struct device* chdev = tdev->chdev;
    return do_read_raw(tdev);
    ;
}

static int
term_read_cano(struct term* tdev)
{
    struct device* chdev = tdev->chdev;
    struct linebuffer* line_in = &tdev->line_in;
    int size = 0;

    while (!(line_in->sflags & ONSTOP)) {
        // move all hold-out content to 'next' buffer
        line_flip(line_in);

        size += do_read_raw_canno(tdev);
        lcntl_transform_inseq(tdev);
    }

    return 0;
}

int
term_read(struct term* tdev)
{
    if ((tdev->lflags & _ICANON)) {
        return term_read_cano(tdev);
    }
    return term_read_noncano(tdev);
}

int
term_flush(struct term* tdev)
{
    if ((tdev->oflags & _OPOST)) {
        lcntl_transform_inseq(tdev);
    }

    struct linebuffer* line_out = &tdev->line_out;
    size_t xmit_len = line_out->current->len;
    void* xmit_buf = line_out->next->buffer;

    rbuffer_gets(line_out->current, xmit_buf, xmit_len);

    off_t off = 0;
    int ret = 0;
    while (xmit_len && ret >= 0) {
        ret = tdev->chdev->ops.write(tdev->chdev, xmit_buf, off, xmit_len);
        xmit_len -= ret;
        off += ret;
    }

    // put back the left over if transmittion went south
    rbuffer_puts(line_out->current, xmit_buf, xmit_len);

    return off;
}