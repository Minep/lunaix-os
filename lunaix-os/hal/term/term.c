#include <hal/term.h>
#include <klibc/string.h>
#include <lunaix/fs.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syslog.h>

#include <usr/lunaix/ioctl_defs.h>

LOG_MODULE("term");

#define termdev(dev) ((struct term*)(dev)->underlay)

#define LCNTL_TABLE_LEN (sizeof(line_controls) / sizeof(struct term_lcntl*))

static struct devclass termdev_class = DEVCLASS(NON, TTY, VTERM);

struct device* sysconsole = NULL;

extern struct termport_pot_ops default_termport_pot_ops;

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
            struct termport_pot_ops* pot_ops;
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

            pot_ops = term->tp_cap->ops;

            if (tios->c_baud != term->iospeed) {
                term->iospeed = tios->c_baud;

                pot_ops->set_speed(term->chdev, tios->c_baud);
            }

            if (old_cf != tios->c_cflag) {
                pot_ops->set_cntrl_mode(term->chdev, tios->c_cflag);
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
    return wrsz;
}

static int
tdev_do_read(struct device* dev, void* buf, off_t fpos, size_t len)
{
    struct term* tdev = termdev(dev);
    lbuf_ref_t current = ref_current(&tdev->line_in);
    bool cont = true;
    size_t rdsz = 0;

    while (cont && rdsz < len) {
        cont = term_read(tdev);
        rdsz += rbuffer_gets(
            deref(current), &((char*)buf)[rdsz], len - rdsz);
    }
    
    tdev->line_in.sflags = 0;
    return rdsz;
}

static cc_t default_cc[_NCCS] = {4, '\n', 0x7f, 3, 1, 24, 22, 0, 0, 1, 1};

static void 
load_default_setting(struct term* tdev) 
{
    tdev->lflags = _ICANON | _IEXTEN | _ISIG | _ECHO | _ECHOE | _ECHONL;
    tdev->iflags = _ICRNL | _IGNBRK;
    tdev->oflags = _ONLCR | _OPOST;
    tdev->iospeed = _B9600;
    memcpy(tdev->cc, default_cc, _NCCS * sizeof(cc_t));
}

static void
alloc_term_buffer(struct term* terminal, size_t sz_hlf) 
{
    line_alloc(&terminal->line_in, sz_hlf);
    line_alloc(&terminal->line_out, sz_hlf);
    terminal->scratch_pad = valloc(sz_hlf);
}

struct termport_potens*
term_attach_potens(struct device* chardev, 
                   struct termport_pot_ops* ops, char* suffix)
{
    struct term* terminal;
    struct device* tdev;
    struct termport_potens* tp_cap;

    terminal = vzalloc(sizeof(struct term));
    if (!terminal) {
        return NULL;
    }

    tdev = device_allocseq(NULL, terminal);
    terminal->dev = tdev;
    terminal->chdev = chardev;

    tdev->ops.read = tdev_do_read;
    tdev->ops.write = tdev_do_write;
    tdev->ops.exec_cmd = term_exec_cmd;

    waitq_init(&terminal->line_in_event);

    alloc_term_buffer(terminal, 1024);

    if (chardev) {
        int cdev_var = DEV_VAR_FROM(chardev->ident.unique);
        register_device(tdev, &termdev_class, "tty%s%d", suffix, cdev_var);
    } else {
        register_device_var(tdev, &termdev_class, "tty");
    }

    INFO("spawned: %s", tdev->name_val);

    tp_cap = new_potens(potens(TERMPORT), struct termport_potens);
    tp_cap->ops = ops ?: &default_termport_pot_ops;

    terminal->tp_cap = tp_cap;
    tp_cap->term = terminal;

    device_grant_potens(tdev, potens_meta(tp_cap));

    load_default_setting(terminal);

    return tp_cap;
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
        signal_send(-tdev->fggrp, signal);
        pwake_all(&tdev->line_in_event);
    }
}