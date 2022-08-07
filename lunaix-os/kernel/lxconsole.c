#include <klibc/string.h>
#include <lunaix/device.h>
#include <lunaix/lxconsole.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/tty/console.h>
#include <lunaix/tty/tty.h>

static struct console lx_console;

int
__tty_write(struct device* dev,
            void* buf,
            unsigned int offset,
            unsigned int len);

void
lxconsole_init()
{
    memset(&lx_console, 0, sizeof(lx_console));
    lx_console.buffer.data = VGA_BUFFER_VADDR + 0x1000;
    lx_console.buffer.size = 8192;
    mutex_init(&lx_console.buffer.lock);

    // 分配控制台缓存
    for (size_t i = 0; i < PG_ALIGN(lx_console.buffer.size); i += PG_SIZE) {
        uintptr_t pa = pmm_alloc_page(KERNEL_PID, 0);
        vmm_set_mapping(PD_REFERENCED,
                        (uintptr_t)lx_console.buffer.data + i,
                        pa,
                        PG_PREM_URW,
                        0);
    }

    memset(lx_console.buffer.data, 0, lx_console.buffer.size);

    lx_console.flush_timer = NULL;

    struct device* tty_dev = device_addseq(NULL, &lx_console, "tty");
    tty_dev->write = __tty_write;
}

int
__tty_write(struct device* dev,
            void* buf,
            unsigned int offset,
            unsigned int len)
{
    struct console* console = (struct console*)dev->underlay;
    console_write(console, buf, len);
}

void
console_schedule_flush()
{
    // TODO make the flush on-demand rather than periodic
}

void
console_view_up()
{
    struct fifo_buffer* buffer = &lx_console.buffer;
    mutex_lock(&buffer->lock);
    size_t p = lx_console.erd_pos - 2;
    while (p < lx_console.erd_pos && p != buffer->wr_pos &&
           ((char*)buffer->data)[p] != '\n') {
        p--;
    }
    p++;

    if (p > lx_console.erd_pos) {
        p = 0;
    }

    buffer->flags |= FIFO_DIRTY;
    lx_console.erd_pos = p;
    mutex_unlock(&buffer->lock);
}

size_t
__find_next_line(size_t start)
{
    size_t p = start;
    while (p != lx_console.buffer.wr_pos &&
           ((char*)lx_console.buffer.data)[p] != '\n') {
        p = (p + 1) % lx_console.buffer.size;
    }
    return p + 1;
}

void
console_view_down()
{
    struct fifo_buffer* buffer = &lx_console.buffer;
    mutex_lock(&buffer->lock);

    lx_console.erd_pos = __find_next_line(lx_console.erd_pos);
    buffer->flags |= FIFO_DIRTY;
    mutex_unlock(&buffer->lock);
}

void
console_flush()
{
    if (mutex_on_hold(&lx_console.buffer.lock)) {
        return;
    }
    if (!(lx_console.buffer.flags & FIFO_DIRTY)) {
        return;
    }

    tty_flush_buffer(lx_console.buffer.data,
                     lx_console.erd_pos,
                     lx_console.buffer.wr_pos,
                     lx_console.buffer.size);
    lx_console.buffer.flags &= ~FIFO_DIRTY;
}

void
console_write(struct console* console, uint8_t* data, size_t size)
{
    mutex_lock(&console->buffer.lock);
    uint8_t* buffer = console->buffer.data;
    uintptr_t ptr = console->buffer.wr_pos;
    uintptr_t rd_ptr = console->buffer.rd_pos;

    char c;
    int lines = 0;
    int j = 0;
    for (size_t i = 0; i < size; i++) {
        c = data[i];
        if (!c) {
            continue;
        }
        buffer[(ptr + j) % console->buffer.size] = c;
        lines += (c == '\n');
        j++;
    }

    size = j;

    uintptr_t new_ptr = (ptr + size) % console->buffer.size;
    console->buffer.wr_pos = new_ptr;

    if (console->lines > TTY_HEIGHT && lines > 0) {
        console->buffer.rd_pos =
          __find_next_line((size + rd_ptr) % console->buffer.size);
    }

    if (new_ptr < ptr + size && new_ptr > rd_ptr) {
        console->buffer.rd_pos = new_ptr;
    }

    console->lines += lines;
    console->erd_pos = console->buffer.rd_pos;
    console->buffer.flags |= FIFO_DIRTY;
    mutex_unlock(&console->buffer.lock);
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