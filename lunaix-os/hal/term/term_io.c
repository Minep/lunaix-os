#include <hal/term.h>

#include <lunaix/clock.h>
#include <lunaix/sched.h>

#include <usr/lunaix/term.h>

#define ONBREAK (LSTATE_EOF | LSTATE_SIGRAISE)
#define ONSTOP (LSTATE_SIGRAISE | LSTATE_EOL | LSTATE_EOF)

static int
do_read_raw(struct term* tdev)
{
    struct linebuffer* line_in;
    lbuf_ref_t current_buf;
    size_t min, sz = 0;
    time_t t, expr, dt = 0;
    
    line_in = &tdev->line_in;
    current_buf = ref_current(line_in);

    min = tdev->cc[_VMIN] - 1;
    t = clock_systime();
    expr = (tdev->cc[_VTIME] * 100) - 1;

    min = MIN(min, (size_t)line_in->sz_hlf);
    while (sz <= min && dt <= expr) {
        // XXX should we held the device lock while we are waiting?
        sched_pass();
        dt = clock_systime() - t;
        t += dt;

        sz = deref(current_buf)->len;
    }

    return 0;
}

static int
term_read_noncano(struct term* tdev)
{
    struct device* chdev = tdev->chdev;
    return do_read_raw(tdev);
}

static int
term_read_cano(struct term* tdev)
{
    struct linebuffer* line_in;

    line_in = &tdev->line_in;
    while (!(line_in->sflags & ONSTOP)) {
        pwait(&tdev->line_in_event);
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
    struct linebuffer* line_out = &tdev->line_out;
    char* xmit_buf = tdev->scratch_pad;
    lbuf_ref_t current_ref = ref_current(line_out);

    int count = 0;

    while (!rbuffer_empty(deref(current_ref))) {
        if ((tdev->oflags & _OPOST)) {
            lcntl_transform_outseq(tdev);
        }

        size_t xmit_len = line_out->current->len;

        rbuffer_gets(line_out->current, xmit_buf, xmit_len);

        off_t off = 0;
        int ret = 0;
        while (xmit_len && ret >= 0) {
            ret = tdev->chdev->ops.write(tdev->chdev, &xmit_buf[off], 0, xmit_len);
            xmit_len -= ret;
            off += ret;
            count += ret;
        }

        // put back the left over if transmittion went south
        rbuffer_puts(line_out->current, xmit_buf, xmit_len);

        line_flip(line_out);
    }

    return count;
}

void
term_notify_data_avaliable(struct termport_capability* cap)
{
    struct term* term;
    struct device* term_chrdev;
    struct linebuffer* line_in;
    char* buf;
    int sz;

    term = cap->term;
    term_chrdev = term->chdev;
    line_in = &term->line_in;
    
    // make room for current buf
    line_flip(line_in);
    buf = line_in->current->buffer;

    sz = term_chrdev->ops.read_async(term_chrdev, buf, 0, line_in->sz_hlf);
    line_in->current->ptr = sz;
    line_in->current->len = sz;

    if ((term->lflags & _ICANON)) {
        lcntl_transform_inseq(term);
        // write all processed to next, and flip back to current
    }

    pwake_all(&term->line_in_event);
}