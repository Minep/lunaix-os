/**
 * @file lxconsole.c
 * @author Lunaixsky (lunaxisky@qq.com)
 * @brief Provides simple terminal support
 * @version 0.1
 * @date 2023-06-18
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <klibc/string.h>
#include <lunaix/device.h>
#include <lunaix/input.h>
#include <lunaix/ioctl.h>
#include <lunaix/keyboard.h>
#include <lunaix/lxconsole.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/sched.h>
#include <lunaix/signal.h>
#include <lunaix/tty/console.h>
#include <lunaix/tty/tty.h>

static struct console lx_console;

int
__tty_write(struct device* dev, void* buf, size_t offset, size_t len);

int
__tty_read(struct device* dev, void* buf, size_t offset, size_t len);

void
console_write(struct console* console, u8_t* data, size_t size);

void
console_flush();

static waitq_t lx_reader;
static volatile char ttychr;

static volatile pid_t fg_pgid = 0;

static inline void
print_control_code(const char cntrl)
{
    console_write_char('^');
    console_write_char(cntrl + 64);
}

int
__lxconsole_listener(struct input_device* dev)
{
    u32_t key = dev->current_pkt.sys_code;
    u32_t type = dev->current_pkt.pkt_type;
    kbd_kstate_t state = key >> 16;
    ttychr = key & 0xff;
    key = key & 0xffff;

    if (type == PKT_RELEASE) {
        goto done;
    }

    if ((state & KBD_KEY_FLCTRL_HELD)) {
        char cntrl = (char)(ttychr | 0x20);
        if ('a' > cntrl || cntrl > 'z') {
            goto done;
        }
        ttychr = cntrl - 'a' + 1;
        switch (ttychr) {
            case TCINTR:
                signal_send(-fg_pgid, _SIGINT);
                print_control_code(ttychr);
                break;
            case TCSTOP:
                signal_send(-fg_pgid, _SIGSTOP);
                print_control_code(ttychr);
                break;
            default:
                break;
        }
    } else if (key == KEY_PG_UP) {
        console_view_up();
        goto done;
    } else if (key == KEY_PG_DOWN) {
        console_view_down();
        goto done;
    } else if ((key & 0xff00) <= KEYPAD) {
        ttychr = key;
    } else {
        goto done;
    }

    pwake_all(&lx_reader);

done:
    return INPUT_EVT_NEXT;
}

int
__tty_exec_cmd(struct device* dev, u32_t req, va_list args)
{
    switch (req) {
        case TIOCGPGRP:
            return fg_pgid;
        case TIOCSPGRP:
            fg_pgid = va_arg(args, pid_t);
            break;
        case TIOCCLSBUF:
            fifo_clear(&lx_console.output);
            fifo_clear(&lx_console.input);
            lx_console.wnd_start = 0;
            lx_console.lines = 0;
            lx_console.output.flags |= FIFO_DIRTY;
            break;
        case TIOCFLUSH:
            lx_console.output.flags |= FIFO_DIRTY;
            console_flush();
            break;
        default:
            return EINVAL;
    }
    return 0;
}

void
lxconsole_init()
{
    memset(&lx_console, 0, sizeof(lx_console));
    fifo_init(&lx_console.output, valloc(8192), 8192, 0);
    fifo_init(&lx_console.input, valloc(4096), 4096, 0);

    lx_console.flush_timer = NULL;
}

int
__tty_write_pg(struct device* dev, void* buf, size_t offset)
{
    return __tty_write(dev, buf, offset, PG_SIZE);
}

int
__tty_read_pg(struct device* dev, void* buf, size_t offset)
{
    return __tty_read(dev, buf, offset, PG_SIZE);
}

int
__tty_write(struct device* dev, void* buf, size_t offset, size_t len)
{
    struct console* console = (struct console*)dev->underlay;
    console_write(console, buf, len);

    return len;
}

int
__tty_read(struct device* dev, void* buf, size_t offset, size_t len)
{
    struct console* console = (struct console*)dev->underlay;

    size_t count = fifo_read(&console->input, buf, len);
    if (count > 0 && ((char*)buf)[count - 1] == '\n') {
        return count;
    }

    while (count < len) {
        pwait(&lx_reader);

        if (ttychr < 0x1B) {
            // ASCII control codes
            switch (ttychr) {
                case TCINTR:
                    fifo_clear(&console->input);
                    return 0;
                case TCBS:
                    if (fifo_backone(&console->input)) {
                        console_write_char(ttychr);
                    }
                    continue;
                case TCLF:
                case TCCR:
                    goto proceed;
                default:
                    break;
            }
            print_control_code(ttychr);
            continue;
        }

    proceed:
        console_write_char(ttychr);
        if (!fifo_putone(&console->input, ttychr) || ttychr == '\n') {
            break;
        }
    }

    return count + fifo_read(&console->input, buf + count, len - count);
}

void
console_schedule_flush()
{
    // TODO make the flush on-demand rather than periodic
}

size_t
__find_next_line(size_t start)
{
    size_t p = start - 1;
    struct fifo_buf* buffer = &lx_console.output;
    do {
        p = (p + 1) % buffer->size;
    } while (p != buffer->wr_pos && ((char*)buffer->data)[p] != '\n');
    return p + (((char*)buffer->data)[p] == '\n');
}

size_t
__find_prev_line(size_t start)
{
    size_t p = start - 1;
    struct fifo_buf* buffer = &lx_console.output;
    do {
        p--;
    } while (p < lx_console.wnd_start && p != buffer->wr_pos &&
             ((char*)buffer->data)[p] != '\n');

    if (p > lx_console.wnd_start) {
        return 0;
    }
    return p + 1;
}

void
console_view_up()
{
    struct fifo_buf* buffer = &lx_console.output;
    mutex_lock(&buffer->lock);
    fifo_set_rdptr(buffer, __find_prev_line(buffer->rd_pos));
    buffer->flags |= FIFO_DIRTY;
    mutex_unlock(&buffer->lock);

    console_flush();
}

void
console_view_down()
{
    struct fifo_buf* buffer = &lx_console.output;
    mutex_lock(&buffer->lock);

    size_t wnd = lx_console.wnd_start;
    size_t p = __find_next_line(buffer->rd_pos);
    fifo_set_rdptr(buffer, p > wnd ? wnd : p);
    buffer->flags |= FIFO_DIRTY;
    mutex_unlock(&buffer->lock);

    console_flush();
}

void
console_flush()
{
    if (mutex_on_hold(&lx_console.output.lock)) {
        return;
    }
    if (!(lx_console.output.flags & FIFO_DIRTY)) {
        return;
    }

    size_t rdpos_save = lx_console.output.rd_pos;
    tty_flush_buffer(&lx_console.output);
    fifo_set_rdptr(&lx_console.output, rdpos_save);

    lx_console.output.flags &= ~FIFO_DIRTY;
}

void
console_write(struct console* console, u8_t* data, size_t size)
{
    struct fifo_buf* fbuf = &console->output;
    mutex_lock(&console->output.lock);
    fifo_set_rdptr(fbuf, console->wnd_start);

    u8_t* buffer = fbuf->data;
    ptr_t ptr = fbuf->wr_pos;
    ptr_t rd_ptr = fbuf->rd_pos;

    char c;
    for (size_t i = 0; i < size; i++) {
        c = data[i];
        if (!c) {
            continue;
        }
        if (c == '\n') {
            console->lines++;
        } else if (c == '\x08') {
            ptr = ptr ? ptr - 1 : fbuf->size - 1;
            continue;
        }
        buffer[ptr] = c;
        ptr = (ptr + 1) % fbuf->size;
    }

    fifo_set_wrptr(fbuf, ptr);

    while (console->lines >= TTY_HEIGHT) {
        rd_ptr = __find_next_line(rd_ptr);
        console->lines--;
    }

    fifo_set_rdptr(&lx_console.output, rd_ptr);
    console->wnd_start = rd_ptr;
    fbuf->flags |= FIFO_DIRTY;
    mutex_unlock(&fbuf->lock);

    if (!lx_console.flush_timer) {
        console_flush();
    }
}

void
console_write_str(char* str)
{
    console_write(&lx_console, (u8_t*)str, strlen(str));
}

void
console_write_char(char str)
{
    console_write(&lx_console, (u8_t*)&str, 1);
}

void
console_start_flushing()
{
    struct lx_timer* timer =
      timer_run_ms(20, console_flush, NULL, TIMER_MODE_PERIODIC);
    lx_console.flush_timer = timer;
}

static int
lxconsole_spawn_ttydev(struct device_def* devdef)
{
    struct device* tty_dev = device_allocseq(NULL, &lx_console);
    tty_dev->ops.write = __tty_write;
    tty_dev->ops.write_page = __tty_write_pg;
    tty_dev->ops.read = __tty_read;
    tty_dev->ops.read_page = __tty_read_pg;
    tty_dev->ops.exec_cmd = __tty_exec_cmd;

    waitq_init(&lx_reader);
    input_add_listener(__lxconsole_listener);

    register_device(tty_dev, &devdef->class, "vcon");

    return 0;
}

static struct device_def lxconsole_def = {
    .name = "Lunaix Virtual Console",
    .class = DEVCLASSV(DEVIF_NON, DEVFN_TTY, DEV_BUILTIN, 12),
    .init = lxconsole_spawn_ttydev
};
// FIXME
EXPORT_DEVICE(lxconsole, &lxconsole_def, load_onboot);