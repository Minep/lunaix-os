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

struct device* sysconsole = NULL;

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
        case TDEV_TCSETCHDEV: {
            int fd = va_arg(args, int);
            struct v_fd* vfd;

            if (vfs_getfd(fd, &vfd)) {
                err = EINVAL;
                goto done;
            }

            struct device* cdev = resolve_device(vfd->file->inode->data);
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
            struct termios* tios = va_arg(args, struct termios*);
            *tios = (struct termios){.c_oflag = term->oflags,
                                      .c_iflag = term->iflags,
                                      .c_lflag = term->lflags,
                                      .c_cflag = term->cflags};
            memcpy(tios->c_cc, term->cc, _NCCS * sizeof(cc_t));
            tios->c_baud = term->iospeed;
        } break;
        case TDEV_TCSETATTR: {
            struct termios* tios = va_arg(args, struct termios*);
            term->iflags = tios->c_iflag;
            term->oflags = tios->c_oflag;
            term->lflags = tios->c_lflag;
            memcpy(term->cc, tios->c_cc, _NCCS * sizeof(cc_t));

            tcflag_t old_cf = term->cflags;
            term->cflags = tios->c_cflag;

            if (!term->tp_cap) {
                goto done;
            }

            if (tios->c_baud != term->iospeed) {
                term->iospeed = tios->c_baud;

                term->tp_cap->set_speed(term->chdev, tios->c_baud);
            }

            if (old_cf != tios->c_cflag) {
                term->tp_cap->set_cntrl_mode(term->chdev, tios->c_cflag);
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
tdev_do_write(struct device* dev, void* buf, off_t fpos, size_t len)
{
    struct term* tdev = termdev(dev);
    lbuf_ref_t current = ref_current(&tdev->line_out);
    size_t wrsz = 0;
    while (wrsz < len) {
        wrsz += rbuffer_puts(
            deref(current), &((char*)buf)[wrsz], len - wrsz);

        if (rbuffer_full(deref(current))) {
            term_flush(tdev);
        }
    }
    
    if (!rbuffer_empty(deref(current))) {
        term_flush(tdev);
    }
    return 0;
}

static int
tdev_do_read(struct device* dev, void* buf, off_t fpos, size_t len)
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

        rdsz += rbuffer_gets(
            deref(current), &((char*)buf)[rdsz], len - rdsz);
    }

    return rdsz;
}

static cc_t default_cc[_NCCS] = {4, '\n', 0x7f, 3, 1, 24, 22, 0, 0, 1, 1};

static void 
load_default_setting(struct term* tdev) 
{
    tdev->lflags = _ICANON | _IEXTEN | _ISIG | _ECHO | _ECHOE | _ECHONL;
    tdev->iflags = _ICRNL | _IGNBRK;
    tdev->oflags = _ONLCR | _OPOST;
    memcpy(tdev->cc, default_cc, _NCCS * sizeof(cc_t));
}

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
    terminal->dev->ops.exec_cmd = term_exec_cmd;

    // TODO choice of lcntl can be flexible
    terminal->lcntl = line_controls[ANSI_LCNTL];

    line_alloc(&terminal->line_in, 1024);
    line_alloc(&terminal->line_out, 1024);

    if (chardev) {
        int cdev_var = DEV_VAR_FROM(chardev->ident.unique);
        register_device(terminal->dev, &termdev, "tty%s%d", suffix, cdev_var);
    } else {
        register_device(terminal->dev, &termdev, "tty%d", termdev.variant++);
    }

    struct capability_meta* termport_cap = device_get_capability(chardev, TERMPORT_CAP);
    if (termport_cap) {
        terminal->tp_cap = get_capability(termport_cap, struct termport_capability);
    }

    struct capability_meta* term_cap = new_capability_marker(TERMIOS_CAP);
    device_grant_capability(terminal->dev, term_cap);

    load_default_setting(terminal);

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