#include <hal/term.h>
#include <klibc/string.h>
#include <lunaix/fs.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>

#include <usr/lunaix/ioctl_defs.h>

#define ANSI_LCNTL 0

extern struct term_lcntl ansi_line_controller;
static struct term_lcntl* line_controls[] = { [ANSI_LCNTL] =
                                                &ansi_line_controller };
#define LCNTL_TABLE_LEN (sizeof(line_controls) / sizeof(struct term_lcntl*))

static int
term_exec_cmd(struct device* dev, u32_t req, va_list args)
{
    struct term* term = (struct term*)dev->underlay;
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
        case TIOCPUSH: {
            u32_t lcntl_idx = va_arg(args, u32_t);
            struct term_lcntl* lcntl = term_get_lcntl(lcntl_idx);

            if (!lcntl) {
                err = EINVAL;
                goto done;
            }

            term_push_lcntl(term, lcntl);
            break;
        }
        case TIOCPOP:
            term_pop_lcntl(term);
            break;
        case TIOCSCDEV: {
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
        case TIOCGCDEV: {
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
        default:
            err = EINVAL;
            goto done;
    }

done:
    device_unlock(dev);
    return err;
}

static int
term_write(struct device* dev, void* buf, size_t offset, size_t len)
{
    struct term* term = (struct term*)dev->underlay;
    size_t sz = MIN(len, term->line.sz_hlf);

    if (!term->chdev) {
        return ENODEV;
    }

    device_lock(term->dev);

    memcpy(term->line.current, &((char*)buf)[offset], sz);

    struct term_lcntl *lcntl, *n;
    llist_for_each(lcntl, n, &term->lcntl_stack, lcntls)
    {
        sz = lcntl->apply(term, term->line.current, sz);
        line_flip(&term->line);
    }

    int errcode = term_sendline(term, sz);

    device_unlock(term->dev);

    return errcode;
}

static int
term_read(struct device* dev, void* buf, size_t offset, size_t len)
{
    struct term* term = (struct term*)dev->underlay;
    size_t sz = MIN(len, term->line.sz_hlf);

    if (!term->chdev) {
        return ENODEV;
    }

    device_lock(term->dev);

    sz = term_readline(term, sz);
    if (!sz) {
        device_unlock(term->dev);
        return 0;
    }

    struct term_lcntl *pos, *n;
    llist_for_each(pos, n, &term->lcntl_stack, lcntls)
    {
        sz = pos->apply(term, term->line.current, sz);
        line_flip(&term->line);
    }

    memcpy(&((char*)buf)[offset], term->line.current, sz);

    device_unlock(term->dev);

    return sz;
}

struct term*
term_create()
{
    struct term* terminal = valloc(sizeof(struct term));

    if (!terminal) {
        return NULL;
    }

    terminal->dev = device_allocseq(NULL, terminal);

    terminal->dev->ops.read = term_read;
    terminal->dev->ops.write = term_write;

    llist_init_head(&terminal->lcntl_stack);
    line_alloc(&terminal->line, 1024);

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

    lcntl_instance->apply = lcntl_template->apply;

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
line_flip(struct linebuffer* lbf)
{
    char* tmp = lbf->current;
    lbf->current = lbf->next;
    lbf->next = tmp;
}

void
line_alloc(struct linebuffer* lbf, size_t sz_hlf)
{
    char* buffer = valloc(sz_hlf * 2);
    lbf->current = buffer;
    lbf->next = &buffer[sz_hlf];
    lbf->sz_hlf = sz_hlf;
}

void
line_free(struct linebuffer* lbf, size_t sz_hlf)
{
    void* ptr = (void*)MIN((ptr_t)lbf->current, (ptr_t)lbf->next);
    vfree(ptr);
}

int
term_sendline(struct term* tdev, size_t len)
{
    struct device* chdev = tdev->chdev;

    device_lock(chdev);

    int code = chdev->ops.write(chdev, tdev->line.current, 0, len);

    device_unlock(chdev);

    return code;
}

int
term_readline(struct term* tdev, size_t len)
{
    struct device* chdev = tdev->chdev;

    device_lock(chdev);

    int code = chdev->ops.read(chdev, tdev->line.current, 0, len);

    device_unlock(chdev);

    return code;
}

int
term_init(struct device_def* devdef)
{
    struct term* terminal = term_create();
    struct device* tdev = device_allocseq(NULL, terminal);
    tdev->ops.read = term_read;
    tdev->ops.write = term_write;
    tdev->ops.exec_cmd = term_exec_cmd;

    devdef->class.variant++;
    device_register(tdev, &devdef->class, "tty%d", devdef->class.variant);

    return 0;
}

static struct device_def vterm_def = {
    .class = DEVCLASS(DEVIF_NON, DEVFN_TTY, DEV_VTERM),
    .name = "virtual terminal",
    .init = term_init
};
EXPORT_DEVICE(vterm, &vterm_def, load_on_demand);