#include <hal/term.h>
#include <klibc/string.h>
#include <lunaix/fs.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>

#include <usr/lunaix/ioctl_defs.h>

#define ANSI_LCNTL 0
#define termdev(dev) ((struct term*)(dev)->underlay)

extern struct term_lcntl ansi_line_controller;
static struct term_lcntl* line_controls[] = {[ANSI_LCNTL] =
                                                 &ansi_line_controller};
#define LCNTL_TABLE_LEN (sizeof(line_controls) / sizeof(struct term_lcntl*))

static struct devclass termdev = DEVCLASS(DEVIF_NON, DEVFN_TTY, DEV_VTERM);

static int
term_exec_cmd(struct device* dev, u32_t req, va_list args)
{
    struct term* term = termdev(dev);
    int err = 0;

    device_lock(dev);

    switch (req) {
    case TIOCSPGRP: {
        pid_t pgid = va_arg(args, pid_t);
        if (pgid < 0) {
            err = EINVAL;
            goto done;
        }
        term->fggrp = pgid;
        break;
    }
    case TIOCGPGRP:
        return term->fggrp;
    case TDEV_TCPUSHLC: {
        u32_t lcntl_idx = va_arg(args, u32_t);
        struct term_lcntl* lcntl = term_get_lcntl(lcntl_idx);

        if (!lcntl) {
            err = EINVAL;
            goto done;
        }

        term_push_lcntl(term, lcntl);
        break;
    }
    case TDEV_TCPOPLC:
        term_pop_lcntl(term);
        break;
    case TDEV_TCSETCHDEV: {
        int fd = va_arg(args, int);
        struct v_fd* vfd;

        if (vfs_getfd(fd, &vfd)) {
            err = EINVAL;
            goto done;
        }

        struct device* cdev = device_cast(vfd->file->inode->data);
        if (!cdev) {
            err = ENOTDEV;
            goto done;
        }
        if (cdev->dev_type != DEV_IFSEQ) {
            err = EINVAL;
            goto done;
        }

        term_bind(term, cdev);
        break;
    }
    case TDEV_TCGETCHDEV: {
        struct dev_info* devinfo = va_arg(args, struct dev_info*);

        if (!term->chdev) {
            err = ENODEV;
            goto done;
        }

        if (devinfo) {
            device_populate_info(term->chdev, devinfo);
        }
        break;
    }
    case TDEV_TCGETATTR: {
        struct _termios* tios = va_arg(args, struct _termios*);
        *tios = (struct _termios){.c_oflag = term->oflags,
                                  .c_iflag = term->iflags,
                                  .c_lflag = term->lflags};
        memcpy(tios->c_cc, term->cc, _NCCS * sizeof(cc_t));
        tios->c_baud = term->iospeed;
    } break;
    case TDEV_TCSETATTR: {
        struct _termios* tios = va_arg(args, struct _termios*);
        term->iflags = tios->c_iflag;
        term->oflags = tios->c_oflag;
        term->lflags = tios->c_lflag;
        memcpy(term->cc, tios->c_cc, _NCCS * sizeof(cc_t));

        if (tios->c_baud != term->iospeed) {
            term->iospeed = tios->c_baud;
            if (!term->chdev_ops.set_speed) {
                goto done;
            }

            term->chdev_ops.set_speed(term->chdev, tios->c_baud);
        }
    } break;
    default:
        err = EINVAL;
        goto done;
    }

done:
    device_unlock(dev);
    return err;
}

static int
tdev_do_write(struct device* dev, void* buf, size_t offset, size_t len)
{
    struct term* tdev = termdev(dev);
    lbuf_ref_t current = ref_current(&tdev->line_out);
    size_t wrsz = 0;
    while (wrsz < len) {
        wrsz += rbuffer_puts(deref(current), &((char*)buf)[offset + wrsz],
                             len - wrsz);
        if (rbuffer_full(deref(current))) {
            term_flush(tdev);
        }
    }

    return 0;
}

static int
tdev_do_read(struct device* dev, void* buf, size_t offset, size_t len)
{
    struct term* tdev = termdev(dev);
    lbuf_ref_t current = ref_current(&tdev->line_in);
    bool cont = true;
    size_t rdsz = 0;
    while (cont && rdsz < len) {
        if (rbuffer_empty(deref(current))) {
            tdev->line_in.sflags = 0;
            cont = term_read(tdev);
        }

        rdsz += rbuffer_gets(deref(current), &((char*)buf)[offset + rdsz],
                             len - rdsz);
    }

    return rdsz;
}

static cc_t default_cc[_NCCS] = {4, '\n', 8, 3, 1, 24, 22, 0, 0, 1, 1};

struct term*
term_create(struct device* chardev, char* suffix)
{
    struct term* terminal = vzalloc(sizeof(struct term));

    if (!terminal) {
        return NULL;
    }

    terminal->dev = device_allocseq(NULL, terminal);
    terminal->chdev = chardev;

    terminal->dev->ops.read = tdev_do_read;
    terminal->dev->ops.write = tdev_do_write;

    llist_init_head(&terminal->lcntl_stack);
    line_alloc(&terminal->line_in, 1024);
    line_alloc(&terminal->line_out, 1024);

    if (chardev) {
        int cdev_var = DEV_VAR_FROM(chardev->ident.unique);
        device_register(terminal->dev, &termdev, "tty%s%d", suffix, cdev_var);
    } else {
        device_register(terminal->dev, &termdev, "tty%d", termdev.variant++);
    }

    terminal->lflags = _ICANON | _IEXTEN | _ISIG | _ECHO;
    terminal->iflags = _ICRNL;
    memcpy(terminal->cc, default_cc, _NCCS * sizeof(cc_t));

    return terminal;
}

int
term_bind(struct term* term, struct device* chdev)
{
    device_lock(term->dev);

    term->chdev = chdev;

    device_unlock(term->dev);

    return 0;
}

struct term_lcntl*
term_get_lcntl(u32_t lcntl_index)
{
    if (lcntl_index >= LCNTL_TABLE_LEN) {
        return NULL;
    }

    struct term_lcntl* lcntl_template = line_controls[lcntl_index];
    struct term_lcntl* lcntl_instance = valloc(sizeof(struct term_lcntl));

    if (!lcntl_instance) {
        return NULL;
    }

    lcntl_instance->process_and_put = lcntl_template->process_and_put;

    return lcntl_instance;
}

int
term_push_lcntl(struct term* term, struct term_lcntl* lcntl)
{
    device_lock(term->dev);

    llist_append(&term->lcntl_stack, &lcntl->lcntls);

    device_unlock(term->dev);

    return 0;
}

int
term_pop_lcntl(struct term* term)
{
    if (term->lcntl_stack.prev == &term->lcntl_stack) {
        return 0;
    }

    device_lock(term->dev);

    struct term_lcntl* lcntl =
        list_entry(term->lcntl_stack.prev, struct term_lcntl, lcntls);
    llist_delete(term->lcntl_stack.prev);

    vfree(lcntl);

    device_unlock(term->dev);

    return 1;
}

void
line_alloc(struct linebuffer* lbf, size_t sz_hlf)
{
    char* buffer = valloc(sz_hlf * 2);
    memset(lbf, 0, sizeof(*lbf));
    lbf->current = rbuffer_create(buffer, sz_hlf);
    lbf->next = rbuffer_create(&buffer[sz_hlf], sz_hlf);
    lbf->sz_hlf = sz_hlf;
}

void
line_free(struct linebuffer* lbf, size_t sz_hlf)
{
    void* ptr =
        (void*)MIN((ptr_t)lbf->current->buffer, (ptr_t)lbf->next->buffer);
    vfree(ptr);
}

void
term_sendsig(struct term* tdev, int signal)
{
    if ((tdev->lflags & _ISIG)) {
        proc_setsignal(get_process(tdev->fggrp), signal);
    }
}