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
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/sched.h>
#include <lunaix/signal.h>
#include <lunaix/tty/tty.h>
#include <lunaix/ds/fifo.h>
#include <lunaix/owloysius.h>

#include <hal/term.h>

struct console
{
    struct lx_timer* flush_timer;
    struct fifo_buf output;
    struct fifo_buf input;
    size_t wnd_start;
    size_t lines;
};

static struct console lx_console;

int
__tty_write(struct device* dev, void* buf, size_t offset, size_t len);

int
__tty_read(struct device* dev, void* buf, size_t offset, size_t len);

void
console_write(struct console* console, u8_t* data, size_t size);

void
console_flush();

void
console_view_up();

void
console_view_down();

static waitq_t lx_reader;

static volatile pid_t fg_pgid = 0;

int
__lxconsole_listener(struct input_device* dev)
{
    u32_t key = dev->current_pkt.sys_code;
    u32_t type = dev->current_pkt.pkt_type;
    kbd_kstate_t state = key >> 16;
    u8_t ttychr = key & 0xff;
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

    fifo_putone(&lx_console.input, ttychr);
    pwake_all(&lx_reader);

done:
    return INPUT_EVT_NEXT;
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
    if (count > 0) {
        return count;
    }

    pwait(&lx_reader);

    return count + fifo_read(&console->input, buf + count, len - count);
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


static void
lxconsole_init()
{
    memset(&lx_console, 0, sizeof(lx_console));
    fifo_init(&lx_console.output, valloc(8192), 8192, 0);
    fifo_init(&lx_console.input, valloc(4096), 4096, 0);

    lx_console.flush_timer = NULL;
}
owloysius_fetch_init(lxconsole_init, on_earlyboot)

static int
lxconsole_spawn_ttydev(struct device_def* devdef)
{
    struct device* tty_dev = device_allocseq(NULL, &lx_console);
    tty_dev->ops.write = __tty_write;
    tty_dev->ops.write_page = __tty_write_pg;
    tty_dev->ops.read = __tty_read;
    tty_dev->ops.read_page = __tty_read_pg;

    waitq_init(&lx_reader);
    input_add_listener(__lxconsole_listener);

    register_device(tty_dev, &devdef->class, "vcon");

    term_create(tty_dev, "FB");

    return 0;
}

static struct device_def lxconsole_def = {
    .name = "Lunaix Virtual Console",
    .class = DEVCLASS(DEVIF_NON, DEVFN_TTY, DEV_BUILTIN),
    .init = lxconsole_spawn_ttydev
};
// FIXME
EXPORT_DEVICE(lxconsole, &lxconsole_def, load_onboot);