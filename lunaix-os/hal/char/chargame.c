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

typedef struct write_cmd {
    int x;
    int y;
    char data[1024];
}write_cmd;

static struct console game_console;

static waitq_t lx_reader;

int
__game_listener(struct input_device* dev)
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
    } else if ((key & 0xff00) <= KEYPAD) {
        ttychr = key;
    } else {
        goto done;
    }

    fifo_putone(&game_console.input, ttychr);
    pwake_all(&lx_reader);

done:
    return INPUT_EVT_NEXT;
}

int
__game_write(struct device* dev, void* buf, size_t offset, size_t len);

int
__game_read(struct device* dev, void* buf, size_t offset, size_t len);

int
__game_write_pg(struct device* dev, void* buf, size_t offset)
{
    return __game_write(dev, buf, offset, 0x1000);
}

int
__game_read_pg(struct device* dev, void* buf, size_t offset)
{
    return __game_read(dev, buf, offset, 0x1000);
}

int
__game_write(struct device* dev, void* buf, size_t offset, size_t len)
{
    static bool first = 1;
    if (first)
    {
        for (int i = 0; i < TTY_HEIGHT; i++)
        {
            tty_clear_line(i);
        }
        first = 0;
        tty_set_cursor(TTY_WIDTH-1, TTY_HEIGHT-1);
    }

    write_cmd *cmd = (write_cmd *)buf;
    if (cmd->x==0x0a0d) {
        cmd->x= 0x0a;
        memcpy(cmd->data, cmd->data+1, TTY_WIDTH+1);
    }else if (cmd->y==0x0a0d) {
        cmd->y = 0xa;
        memcpy(cmd->data, cmd->data+1, TTY_WIDTH+1);
    }
    
    tty_put_str_at((char*)&(cmd->data), cmd->x, cmd->y);
    
    return len;
}

int
__game_read(struct device* dev, void* buf, size_t offset, size_t len)
{
    struct console* console = (struct console*)dev->underlay;
    size_t count = fifo_read(&console->input, buf, len);

    if (count > 0) {
        return count;
    }

    pwait(&lx_reader);

    return count + fifo_read(&console->input, buf + count, len - count);
}

static int
chargame_init(struct device_def* devdef)
{
    struct device* tty_dev = device_allocseq(NULL, &game_console);
    tty_dev->ops.write = __game_write;
    tty_dev->ops.write_page = __game_write_pg;
    tty_dev->ops.read = __game_read;
    tty_dev->ops.read_page = __game_read_pg;
    tty_dev->ops.read_async = __game_read;

    waitq_init(&lx_reader);
    input_add_listener(__game_listener);

    register_device(tty_dev, &devdef->class, "game");

    term_create(tty_dev, "G");

    memset(&game_console, 0, sizeof(game_console));
    fifo_init(&game_console.output, valloc(8192), 8192, 0);
    fifo_init(&game_console.input, valloc(4096), 4096, 0);

    game_console.flush_timer = NULL;

    return 0;
}

static struct device_def chargame_def = {
    .name = "Character Game Console",
    .class = DEVCLASS(DEVIF_NON, DEVFN_TTY, DEV_BUILTIN),
    .init = chargame_init,
};
// FIXME
EXPORT_DEVICE(chargame, &chargame_def, load_onboot);