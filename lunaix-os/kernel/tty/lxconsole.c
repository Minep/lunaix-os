#include <klibc/string.h>
#include <lunaix/device.h>
#include <lunaix/input.h>
#include <lunaix/keyboard.h>
#include <lunaix/lxconsole.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/sched.h>
#include <lunaix/tty/console.h>
#include <lunaix/tty/tty.h>

static struct console lx_console;

int
__tty_write(struct device* dev, void* buf, size_t offset, size_t len);

int
__tty_read(struct device* dev, void* buf, size_t offset, size_t len);

static waitq_t lx_reader;
static volatile char key;

int
__lxconsole_listener(struct input_device* dev)
{
    uint32_t keycode = dev->current_pkt.sys_code;
    uint32_t type = dev->current_pkt.pkt_type;
    if (type == PKT_PRESS) {
        if (keycode == KEY_PG_UP) {
            console_view_up();
        } else if (keycode == KEY_PG_DOWN) {
            console_view_down();
        }
        goto done;
    }
    if ((keycode & 0xff00) > KEYPAD) {
        goto done;
    }

    key = (char)(keycode & 0x00ff);

    pwake_all(&lx_reader);

done:
    return INPUT_EVT_NEXT;
}

void
lxconsole_init()
{
    memset(&lx_console, 0, sizeof(lx_console));
    fifo_init(&lx_console.output, valloc(8192), 8192, 0);
    fifo_init(&lx_console.input, valloc(4096), 4096, 0);

    lx_console.flush_timer = NULL;

    struct device* tty_dev = device_addseq(NULL, &lx_console, "tty");
    tty_dev->write = __tty_write;
    tty_dev->read = __tty_read;

    waitq_init(&lx_reader);
    input_add_listener(__lxconsole_listener);
}

int
__tty_write(struct device* dev, void* buf, size_t offset, size_t len)
{
    struct console* console = (struct console*)dev->underlay;
    console_write(console, buf, len);
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

        if (key == 0x08) {
            if (fifo_backone(&console->input)) {
                console_write_char(key);
            }
            continue;
        }
        console_write_char(key);
        if (!fifo_putone(&console->input, key) || key == '\n') {
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
console_write(struct console* console, uint8_t* data, size_t size)
{
    struct fifo_buf* fbuf = &console->output;
    mutex_lock(&console->output.lock);
    fifo_set_rdptr(fbuf, console->wnd_start);

    uint8_t* buffer = fbuf->data;
    uintptr_t ptr = fbuf->wr_pos;
    uintptr_t rd_ptr = fbuf->rd_pos;

    char c;
    int j = 0;
    for (size_t i = 0; i < size; i++) {
        c = data[i];
        if (!c) {
            continue;
        }
        if (c == '\n') {
            console->lines++;
        }
        buffer[(ptr + j) % fbuf->size] = c;
        j++;
    }

    fifo_set_wrptr(fbuf, (ptr + j) % fbuf->size);

    while (console->lines >= TTY_HEIGHT) {
        rd_ptr = __find_next_line(rd_ptr);
        console->lines--;
    }

    fifo_set_rdptr(&lx_console.output, rd_ptr);
    console->wnd_start = rd_ptr;
    fbuf->flags |= FIFO_DIRTY;
    mutex_unlock(&fbuf->lock);
}

void
console_write_str(char* str)
{
    console_write(&lx_console, str, strlen(str));
}

void
console_write_char(char str)
{
    console_write(&lx_console, &str, 1);
}

void
console_start_flushing()
{
    struct lx_timer* timer =
      timer_run_ms(20, console_flush, NULL, TIMER_MODE_PERIODIC);
    lx_console.flush_timer = timer;
}