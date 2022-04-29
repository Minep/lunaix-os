#include <lunaix/peripheral/ps2kbd.h>
#include <lunaix/clock.h>
#include <lunaix/timer.h>
#include <lunaix/common.h>
#include <lunaix/syslog.h>

#include <hal/cpu.h>
#include <hal/ioapic.h>

#include <arch/x86/interrupts.h>
#include <stdint.h>
#include <klibc/string.h>

#define PS2_DEV_CMD_MAX_ATTEMPTS 5

LOG_MODULE("PS2KBD");

static struct ps2_cmd_queue cmd_q;
static struct ps2_key_buffer key_buf;
static struct ps2_kbd_state kbd_state;

#define KEY_NUM(x)      (x + 0x30)
#define KEY_NPAD(x)      ON_KEYPAD(KEY_NUM(x))

// 我们使用 Scancode Set 2

// 大部分的扫描码（键码）
static kbd_keycode scancode_set2[] = {
    0, KEY_F9, 0, KEY_F5, KEY_F3, KEY_F1, KEY_F2, KEY_F12, 0, KEY_F10, KEY_F8, KEY_F6,
    KEY_F4, KEY_HTAB, '`', 0, 0, KEY_LALT, KEY_LSHIFT, 0, KEY_LCTRL, 'q', KEY_NUM(1), 
    0, 0, 0, 'z', 's', 'a', 'w', KEY_NUM(2), 0, 0, 'c', 'x', 'd', 'e', KEY_NUM(4), KEY_NUM(3), 
    0, 0, KEY_SPACE, 'v', 'f', 't', 'r', KEY_NUM(5),
    0, 0, 'n', 'b', 'h', 'g', 'y', KEY_NUM(6), 0, 0, 0, 'm', 'j', 'u', KEY_NUM(7), KEY_NUM(8),
    0, 0, ',', 'k', 'i', 'o', KEY_NUM(0), KEY_NUM(9), 0, 0, '.', '/', 'l', ';', 'p', '-', 0, 0,
    0, '\'', 0, '[', '=', 0, 0, KEY_CAPSLK, KEY_RSHIFT, KEY_LF, ']', 0, '\\', 0, 0, 0, 0, 0, 0, 0,
    0, KEY_BS, 0, 0, KEY_NPAD(1), 0, KEY_NPAD(4), KEY_NPAD(7), 0, 0, 0, KEY_NPAD(0), ON_KEYPAD('.'),
    KEY_NPAD(2), KEY_NPAD(5), KEY_NPAD(6), KEY_NPAD(8), KEY_ESC, KEY_NUMSLK, KEY_F11, ON_KEYPAD('+'),
    KEY_NPAD(3), ON_KEYPAD('-'), ON_KEYPAD('*'), KEY_NPAD(9), KEY_SCRLLK, 0, 0, 0, 0, KEY_F7
};

// 一些特殊的键码（以 0xe0 位前缀的）
static kbd_keycode scancode_set2_ex[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KEY_RALT, 0, 0,
    KEY_RCTRL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ON_KEYPAD('/'), 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, ON_KEYPAD(KEY_LF), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    KEY_END, 0, KEY_LEFT, KEY_HOME,
    0, 0, 0, KEY_INSERT, KEY_DELETE, KEY_DOWN, 0, KEY_RIGHT, KEY_UP, 0, 0,
    0, 0, KEY_PG_DOWN, 0, 0, KEY_PG_UP
};

// 用于处理 Shift+<key> 的情况
static kbd_keycode scancode_set2_shift[] = {
    0, KEY_F9, 0, KEY_F5, KEY_F3, KEY_F1, KEY_F2, KEY_F12, 0, KEY_F10, KEY_F8, KEY_F6,
    KEY_F4, KEY_HTAB, '~', 0, 0, KEY_LALT, KEY_LSHIFT, 0, KEY_LCTRL, 'Q', '!', 
    0, 0, 0, 'Z', 'S', 'A', 'W', '@', 0, 0, 'C', 'X', 'D', 'E', '$', '#', 
    0, 0, KEY_SPACE, 'V', 'F', 'T', 'R', '%',
    0, 0, 'N', 'B', 'H', 'G', 'Y', '^', 0, 0, 0, 'M', 'J', 'U', '&', '*',
    0, 0, '<', 'K', 'I', 'O', ')', '(', 0, 0, '>', '？', 'L', ':', 'P', '_', 0, 0,
    0, '"', '{', '+', 0, 0, KEY_CAPSLK, KEY_RSHIFT, KEY_LF, '}', 0, '|', 0, 0, 0, 0, 0, 0, 0,
    0, KEY_BS, 0, 0, KEY_NPAD(1), 0, KEY_NPAD(4), KEY_NPAD(7), 0, 0, 0, KEY_NPAD(0), ON_KEYPAD('.'),
    KEY_NPAD(2), KEY_NPAD(5), KEY_NPAD(6), KEY_NPAD(8), KEY_ESC, KEY_NUMSLK, KEY_F11, ON_KEYPAD('+'),
    KEY_NPAD(3), ON_KEYPAD('-'), ON_KEYPAD('*'), KEY_SCRLLK, 0, 0, 0, 0, KEY_F7
};


#define KBD_STATE_WAIT_KEY 0
#define KBD_STATE_SPECIAL 1
#define KBD_STATE_RELEASED 2

void intr_ps2_kbd_handler(const isr_param* param);

void ps2_device_post_cmd(char cmd, char arg) {
    // FIXME: Need a mutex lock on this.
    int index = (cmd_q.queue_ptr + cmd_q.queue_len) % PS2_CMD_QUEUE_SIZE;
    int diff = index - cmd_q.queue_ptr;
    if (diff > 0 && diff != cmd_q.queue_len) {
        // 队列已满！
        return;
    }

    struct ps2_cmd *container = &cmd_q.cmd_queue[index];
    container->cmd = cmd;
    container->arg = arg;
    cmd_q.queue_len++;
}

void ps2_kbd_init() {

    memset(&cmd_q, 0, sizeof(cmd_q));
    memset(&key_buf, 0, sizeof(key_buf));
    memset(&kbd_state, 0, sizeof(kbd_state));
    kbd_state.translation_table = scancode_set2;
    kbd_state.state = KBD_STATE_WAIT_KEY;

    cpu_disable_interrupt();

    // XXX: 是否需要使用FADT探测PS/2控制器的存在？

    // 1、禁用任何的PS/2设备
    ps2_post_cmd(PS2_CMD_PORT1_DISABLE, PS2_NO_ARG);
    ps2_post_cmd(PS2_CMD_PORT2_DISABLE, PS2_NO_ARG);
    
    // 2、清空控制器缓冲区
    io_inb(PS2_PORT_DATA);

    char result;

    // 3、屏蔽所有PS/2设备（端口1&2）IRQ，并且禁用键盘键码转换功能
    result = ps2_issue_cmd(PS2_CMD_READ_CFG, PS2_NO_ARG);
    result = result & ~(PS2_CFG_P1INT | PS2_CFG_P2INT | PS2_CFG_TRANSLATION);
    ps2_post_cmd(PS2_CMD_WRITE_CFG, result);

    // 4、控制器自检
    result = ps2_issue_cmd(PS2_CMD_SELFTEST, PS2_NO_ARG);
    if (result != PS2_RESULT_TEST_OK) {
        kprintf(KERROR "Controller self-test failed.");
        goto done;
    }

    // 5、设备自检（端口1自检，通常是我们的键盘）
    result = ps2_issue_cmd(PS2_CMD_SELFTEST_PORT1, PS2_NO_ARG);
    if (result != 0) {
        kprintf(KERROR "Interface test on port 1 failed.");
        goto done;
    }

    // 6、开启位于端口1的 IRQ，并启用端口1。不用理会端口2，那儿一般是鼠标。
    ps2_post_cmd(PS2_CMD_PORT1_ENABLE, PS2_NO_ARG);
    result = ps2_issue_cmd(PS2_CMD_READ_CFG, PS2_NO_ARG);
    result = result | PS2_CFG_P1INT;
    ps2_post_cmd(PS2_CMD_WRITE_CFG, result);

    // 至此，PS/2控制器和设备已完成初始化，可以正常使用。

    // 将我们的键盘驱动挂载到第204号中断上（已由IOAPIC映射至IRQ#1），
    intr_subscribe(PC_KBD_IV, intr_ps2_kbd_handler);

    // 搞一个计时器，将我们的 ps2_process_cmd 挂上去。每隔5毫秒执行排在队头的命令。
    //  为什么只执行队头的命令，而不是全部的命令？
    //      因为我们需要保证isr尽量的简短，运行起来快速。而发送这些命令非常的耗时。
    timer_run_ms(5, ps2_process_cmd, NULL, TIMER_MODE_PERIODIC);

done:
    cpu_enable_interrupt();
}

void ps2_process_cmd(void* arg) {
    // FIXME: Need a mutex lock on this.
    // if lock is hold by other, then it just simply return (not wait! as we are in isr).
    if (!cmd_q.queue_len) {
        return;
    }

    // 处理队列排头的指令
    struct ps2_cmd *pending_cmd = &cmd_q.cmd_queue[cmd_q.queue_ptr];
    char result;
    int attempts = 0;

    // 尝试将命令发送至PS/2键盘（通过PS/2控制器）
    // 如果不成功（0x60 IO口返回 0xfe，即 NAK 或 Resend）
    // 则尝试最多五次
    do {
        result = ps2_issue_cmd(pending_cmd->cmd, pending_cmd->arg);
        attempts++;
    } while(result == PS2_RESULT_NAK && attempts < PS2_DEV_CMD_MAX_ATTEMPTS);
    
    // TODO: 是否需要处理不成功的指令？

    cmd_q.queue_ptr = (cmd_q.queue_ptr + 1) % PS2_CMD_QUEUE_SIZE;
    cmd_q.queue_len--;
}

void kbd_buffer_key_event(kbd_keycode key, uint8_t scancode, kbd_kstate_t state) {
    if (key == KEY_CAPSLK) {
        kbd_state.key_state = kbd_state.key_state ^ (KBD_KEY_CAPSLKED & -state);
    } else if (key == KEY_NUMSLK) {
        kbd_state.key_state = kbd_state.key_state ^ (KBD_KEY_NUMBLKED & -state);
    } else if (key == KEY_SCRLLK) {
        kbd_state.key_state = kbd_state.key_state ^ (KBD_KEY_SCRLLKED & -state);
    } else {
        state = state | kbd_state.key_state;
        time_t timestamp = clock_systime();
        // TODO: Construct the packet.
        kprintf(KDEBUG "%c (t=%d, s=%x, c=%d)\n", key & 0x00ff, timestamp, state, (key & 0xff00) >> 8);
        return; // do not delete this return
    }

    ps2_device_post_cmd(PS2_KBD_CMD_SETLED, kbd_state.key_state >> 1);
}

void intr_ps2_kbd_handler(const isr_param* param) {
    uint8_t scancode = io_inb(PS2_PORT_DATA) & 0xff;
    kbd_keycode key;

    kprintf(KINFO "%x\n", scancode & 0xff);
    
    // FIXME: 实现 Shift+<key> 
    switch (kbd_state.state)
    {
    case KBD_STATE_WAIT_KEY:
        if (scancode == 0xf0) { // release code
            kbd_state.state = KBD_STATE_RELEASED;       
        } else if (scancode == 0xe0) {
            kbd_state.state = KBD_STATE_SPECIAL;
            kbd_state.translation_table = scancode_set2_ex;
        } else {
            key = kbd_state.translation_table[scancode];
            kbd_buffer_key_event(key, scancode, KBD_KEY_PRESSED);
        }
        break;
    case KBD_STATE_SPECIAL:
        if (scancode == 0xf0) { //release code
            kbd_state.state = KBD_STATE_RELEASED;       
        } else {
            key = kbd_state.translation_table[scancode];
            kbd_buffer_key_event(key, scancode, KBD_KEY_PRESSED);

            kbd_state.state = KBD_STATE_WAIT_KEY;
            kbd_state.translation_table = scancode_set2;
        }
        break;
    case KBD_STATE_RELEASED:
        key = kbd_state.translation_table[scancode];
        kbd_buffer_key_event(key, scancode, KBD_KEY_RELEASED);
        
        // reset the translation table to scancode_set2
        kbd_state.translation_table = scancode_set2;
        kbd_state.state = KBD_STATE_WAIT_KEY;   
        break;
    
    default:
        break;
    }
}

static uint8_t ps2_issue_cmd(char cmd, uint16_t arg) {
    ps2_post_cmd(cmd, arg);

    char result;
    
    // 等待PS/2控制器返回。通过轮询（polling）状态寄存器的 bit 0
    // 如置位，则表明返回代码此时就在 0x60 IO口上等待读取。
    while(!((result = io_inb(PS2_PORT_STATUS)) & PS2_STATUS_OFULL));

    return io_inb(PS2_PORT_DATA);
}

static void ps2_post_cmd(char cmd, uint16_t arg) {
    char result;
    // 等待PS/2输入缓冲区清空，这样我们才可以写入命令
    while((result = io_inb(PS2_PORT_STATUS)) & PS2_STATUS_IFULL);

    io_outb(PS2_PORT_CMDREG, cmd);
    if (!(arg & PS2_NO_ARG)) {
        io_outb(PS2_PORT_DATA, (uint8_t)(arg & 0x00ff));
    }
}
