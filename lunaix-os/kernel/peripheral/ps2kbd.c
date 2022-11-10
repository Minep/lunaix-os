#include <hal/acpi/acpi.h>
#include <hal/ioapic.h>
#include <lunaix/clock.h>
#include <lunaix/common.h>
#include <lunaix/input.h>
#include <lunaix/isrm.h>
#include <lunaix/peripheral/ps2kbd.h>
#include <lunaix/syslog.h>
#include <lunaix/timer.h>

#include <arch/x86/interrupts.h>
#include <hal/cpu.h>
#include <klibc/string.h>

#include <stdint.h>

#define PS2_DEV_CMD_MAX_ATTEMPTS 5

LOG_MODULE("i8042");

static struct ps2_cmd_queue cmd_q;
static struct ps2_kbd_state kbd_state;

#define KEY_NUM(x) (x + 0x30)
#define KEY_NPAD(x) ON_KEYPAD(KEY_NUM(x))

// 我们使用 Scancode Set 2

// clang-format off

// 大部分的扫描码（键码）
static kbd_keycode_t scancode_set2[] = {
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
static kbd_keycode_t scancode_set2_ex[] = {
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
static kbd_keycode_t scancode_set2_shift[] = {
    0, KEY_F9, 0, KEY_F5, KEY_F3, KEY_F1, KEY_F2, KEY_F12, 0, KEY_F10, KEY_F8, KEY_F6,
    KEY_F4, KEY_HTAB, '~', 0, 0, KEY_LALT, KEY_LSHIFT, 0, KEY_LCTRL, 'Q', '!', 
    0, 0, 0, 'Z', 'S', 'A', 'W', '@', 0, 0, 'C', 'X', 'D', 'E', '$', '#', 
    0, 0, KEY_SPACE, 'V', 'F', 'T', 'R', '%',
    0, 0, 'N', 'B', 'H', 'G', 'Y', '^', 0, 0, 0, 'M', 'J', 'U', '&', '*',
    0, 0, '<', 'K', 'I', 'O', ')', '(', 0, 0, '>', '?', 'L', ':', 'P', '_', 0, 0,
    0, '"', 0, '{', '+', 0, 0, KEY_CAPSLK, KEY_RSHIFT, KEY_LF, '}', 0, '|', 0, 0, 0, 0, 0, 0, 0,
    0, KEY_BS, 0, 0, KEY_NPAD(1), 0, KEY_NPAD(4), KEY_NPAD(7), 0, 0, 0, KEY_NPAD(0), ON_KEYPAD('.'),
    KEY_NPAD(2), KEY_NPAD(5), KEY_NPAD(6), KEY_NPAD(8), KEY_ESC, KEY_NUMSLK, KEY_F11, ON_KEYPAD('+'),
    KEY_NPAD(3), ON_KEYPAD('-'), ON_KEYPAD('*'), KEY_SCRLLK, 0, 0, 0, 0, KEY_F7
};

// clang-format on

static struct input_device* kbd_idev;

#define KBD_STATE_KWAIT 0x00
#define KBD_STATE_KSPECIAL 0x01
#define KBD_STATE_KRELEASED 0x02
#define KBD_STATE_E012 0x03
#define KBD_STATE_KRELEASED_E0 0x04
#define KBD_STATE_CMDPROCS 0x40

// #define KBD_ENABLE_SPIRQ_FIX
#define KBD_ENABLE_SPIRQ_FIX2
// #define KBD_DBGLOG

void
intr_ps2_kbd_handler(const isr_param* param);

static uint8_t
ps2_issue_cmd_wretry(char cmd, uint16_t arg);

void
ps2_device_post_cmd(char cmd, char arg)
{
    mutex_lock(&cmd_q.mutex);
    int index = (cmd_q.queue_ptr + cmd_q.queue_len) % PS2_CMD_QUEUE_SIZE;
    if (index == cmd_q.queue_ptr && cmd_q.queue_len) {
        // 队列已满！
        mutex_unlock(&cmd_q.mutex);
        return;
    }

    struct ps2_cmd* container = &cmd_q.cmd_queue[index];
    container->cmd = cmd;
    container->arg = arg;
    cmd_q.queue_len++;

    // 释放锁，同理。
    mutex_unlock(&cmd_q.mutex);
}

void
ps2_kbd_init()
{

    memset(&cmd_q, 0, sizeof(cmd_q));
    memset(&kbd_state, 0, sizeof(kbd_state));

    mutex_init(&cmd_q.mutex);

    kbd_state.translation_table = scancode_set2;
    kbd_state.state = KBD_STATE_KWAIT;

    kbd_idev = input_add_device("i8042-kbd");

    acpi_context* acpi_ctx = acpi_get_context();
    if (acpi_ctx->fadt.header.rev > 1) {
        /*
         *  只有当前ACPI版本大于1时，我们才使用FADT的IAPC_BOOT_ARCH去判断8042是否存在。
         *  这是一个坑，在ACPI v1中，这个字段是reserved！而这及至ACPI v2才出现。
         *  需要注意：Bochs 和 QEMU 使用的是ACPI v1，而非 v2
         * （virtualbox好像是v4）
         *
         *  (2022/6/29)
         *  QEMU在7.0.0版本中，修复了FADT::IAPC_BOOT无法正确提供关于i8042的信息的bug
         *      https://wiki.qemu.org/ChangeLog/7.0#ACPI_.2F_SMBIOS
         *
         *  请看Bochs的bios源码（QEMU的BIOS其实是照抄bochs的，所以也是一个德行。。）：
         *      https://bochs.sourceforge.io/cgi-bin/lxr/source/bios/rombios32.c#L1314
         */
        if (!(acpi_ctx->fadt.boot_arch & IAPC_ARCH_8042)) {
            kprintf(KERROR "not found\n");
            // FUTURE: Some alternative fallback on this? Check PCI bus for USB
            // controller instead?
            return;
        }
    } else {
        kprintf(KWARN "outdated FADT used, assuming exists.\n");
    }

    char result;

    cpu_disable_interrupt();

    // 1、禁用任何的PS/2设备
    ps2_post_cmd(PS2_PORT_CTRL_CMDREG, PS2_CMD_PORT1_DISABLE, PS2_NO_ARG);
    ps2_post_cmd(PS2_PORT_CTRL_CMDREG, PS2_CMD_PORT2_DISABLE, PS2_NO_ARG);

    // 2、清空控制器缓冲区
    io_inb(PS2_PORT_ENC_DATA);

    // 3、屏蔽所有PS/2设备（端口1&2）IRQ，并且禁用键盘键码转换功能
    result = ps2_issue_cmd(PS2_CMD_READ_CFG, PS2_NO_ARG);
    result = result & ~(PS2_CFG_P1INT | PS2_CFG_P2INT);
    ps2_post_cmd(PS2_PORT_CTRL_CMDREG, PS2_CMD_WRITE_CFG, result);

    // 4、控制器自检
    result = ps2_issue_cmd_wretry(PS2_CMD_SELFTEST, PS2_NO_ARG);
    if (result != PS2_RESULT_TEST_OK) {
        kprintf(KWARN "controller self-test failed. (%x)\n", result);
        // goto done;
    }

    // 5、设备自检（端口1自检，通常是我们的键盘）
    result = ps2_issue_cmd_wretry(PS2_CMD_SELFTEST_PORT1, PS2_NO_ARG);
    if (result != 0) {
        kprintf(KERROR "interface test on port 1 failed. (%x)\n", result);
        // goto done;
    }

    ps2_post_cmd(PS2_PORT_CTRL_CMDREG, PS2_CMD_PORT2_DISABLE, PS2_NO_ARG);

    // 6、开启位于端口1的 IRQ，并启用端口1。不用理会端口2，那儿一般是鼠标。
    ps2_post_cmd(PS2_PORT_CTRL_CMDREG, PS2_CMD_PORT1_ENABLE, PS2_NO_ARG);
    result = ps2_issue_cmd(PS2_CMD_READ_CFG, PS2_NO_ARG);
    // 重新设置配置字节，因为控制器自检有可能重置我们先前做的改动。
    result = (result | PS2_CFG_P1INT) & ~(PS2_CFG_TRANSLATION | PS2_CFG_P2INT);
    ps2_post_cmd(PS2_PORT_CTRL_CMDREG, PS2_CMD_WRITE_CFG, result);

    // 至此，PS/2控制器和设备已完成初始化，可以正常使用。

    // 搞一个计时器，将我们的 ps2_process_cmd
    // 挂上去。每隔5毫秒执行排在队头的命令。
    //  为什么只执行队头的命令，而不是全部的命令？
    //      因为我们需要保证isr尽量的简短，运行起来快速。而发送这些命令非常的耗时。
    timer_run_ms(5, ps2_process_cmd, NULL, TIMER_MODE_PERIODIC);

    /*
     *   一切准备就绪后，我们才教ioapic去启用IRQ#1。
     *   至于为什么要在这里，原因是：初始化所使用的一些指令可能会导致IRQ#1的触发（因为返回码），或者是一些什么
     *  情况导致IRQ#1的误触发（可能是未初始化导致IRQ#1线上不稳定）。于是这些IRQ#1会堆积在APIC的队列里（因为此时我们正在
     *  初始化8042，屏蔽了所有中断，IF=0）。
     *  当sti后，这些堆积的中断会紧跟着递送进CPU里，导致我们的键盘handler误认为由按键按下，从而将这个毫无意义的数值加入
     *  我们的队列中，以供上层读取。
     *
     *  所以，保险的方法是：在初始化后才去设置ioapic，这样一来我们就能有一个稳定的IRQ#1以放心使用。
     */
    isrm_bindirq(PC_AT_IRQ_KBD, intr_ps2_kbd_handler);

done:
    cpu_enable_interrupt();
}

void
ps2_process_cmd(void* arg)
{
    /*
     * 检查锁是否已被启用，如果启用，则表明该timer中断发生时，某个指令正在入队。
     * 如果是这种情况则跳过，留到下一轮再尝试处理。
     * 注意，这里其实是ISR的一部分（timer中断），对于单核CPU来说，ISR等同于单个的原子操作。
     * （因为EFLAGS.IF=0，所有可屏蔽中断被屏蔽。对于NMI的情况，那么就直接算是triple
     * fault了，所以也没有讨论的意义）
     * 所以，假若我们遵从互斥锁的严格定义（即这里需要阻塞），那么中断将会被阻塞，进而造成死锁。
     * 因此，我们这里仅仅进行判断。
     * 会不会产生指令堆积？不会，因为指令发送的频率远远低于指令队列清空的频率。在目前，我们发送的唯一指令
     * 就只是用来开关键盘上的LED灯（如CAPSLOCK）。
     */
    if (mutex_on_hold(&cmd_q.mutex) || !cmd_q.queue_len) {
        return;
    }

    // 处理队列排头的指令
    struct ps2_cmd* pending_cmd = &cmd_q.cmd_queue[cmd_q.queue_ptr];
    char result;
    int attempts = 0;

    // 尝试将命令发送至PS/2键盘（通过PS/2控制器）
    // 如果不成功（0x60 IO口返回 0xfe，即 NAK i.e. Resend）
    // 则尝试最多五次
    do {
        result = ps2_issue_dev_cmd(pending_cmd->cmd, pending_cmd->arg);
#ifdef KBD_ENABLE_SPIRQ_FIX
        kbd_state.state += KBD_STATE_CMDPROCS;
#endif
        attempts++;
    } while (result == PS2_RESULT_NAK && attempts < PS2_DEV_CMD_MAX_ATTEMPTS);
    // XXX: 是否需要处理不成功的指令？

    cmd_q.queue_ptr = (cmd_q.queue_ptr + 1) % PS2_CMD_QUEUE_SIZE;
    cmd_q.queue_len--;
}

void
kbd_buffer_key_event(kbd_keycode_t key, uint8_t scancode, kbd_kstate_t state)
{
    /*
        forgive me on these ugly bit-level tricks,
        I really hate doing branching on these "fliping switch" things
    */
    if (key == KEY_CAPSLK) {
        kbd_state.key_state ^= KBD_KEY_FCAPSLKED & -state;
    } else if (key == KEY_NUMSLK) {
        kbd_state.key_state ^= KBD_KEY_FNUMBLKED & -state;
    } else if (key == KEY_SCRLLK) {
        kbd_state.key_state ^= KBD_KEY_FSCRLLKED & -state;
    } else {
        if ((key & MODIFR)) {
            kbd_kstate_t tmp = (KBD_KEY_FLSHIFT_HELD << (key & 0x00ff));
            kbd_state.key_state = (kbd_state.key_state & ~tmp) | (tmp & -state);
        } else if (!(key & 0xff00) &&
                   (kbd_state.key_state &
                    (KBD_KEY_FLSHIFT_HELD | KBD_KEY_FRSHIFT_HELD))) {
            key = scancode_set2_shift[scancode];
        }
        state = state | kbd_state.key_state;
        key = key & (0xffdf |
                     -('a' > key || key > 'z' || !(state & KBD_KEY_FCAPSLKED)));

        struct input_evt_pkt ipkt = { .pkt_type = (state & KBD_KEY_FPRESSED)
                                                    ? PKT_PRESS
                                                    : PKT_RELEASE,
                                      .scan_code = scancode,
                                      .sys_code = (state << 16) | key };

        input_fire_event(kbd_idev, &ipkt);

        return;
    }

    if (state & KBD_KEY_FPRESSED) {
        // Ooops, this guy generates irq!
        ps2_device_post_cmd(PS2_KBD_CMD_SETLED,
                            (kbd_state.key_state >> 1) & 0x00ff);
    }
}

void
intr_ps2_kbd_handler(const isr_param* param)
{

    // This is important! Don't believe me? try comment it out and run on Bochs!
    // while (!(io_inb(PS2_PORT_CTRL_STATUS) & PS2_STATUS_OFULL))
    //    ;

    // I know you are tempting to move this chunk after the keyboard state
    // check. But DO NOT. This chunk is in right place and right order. Moving
    // it at your own risk This is to ensure we've cleared the output buffer
    // everytime, so it won't pile up across irqs.
    uint8_t scancode = io_inb(PS2_PORT_ENC_DATA);
    kbd_keycode_t key;

    /*
     *    判断键盘是否处在指令发送状态，防止误触发。（伪输入中断）
     * 这是因为我们需要向ps/2设备发送指令（比如控制led灯），而指令会有返回码。
     * 这就会有可能导致ps/2控制器在受到我们的命令后（在ps2_process_cmd中），
     * 产生IRQ#1中断（虽然说这种情况取决于底层BIOS实现，但还是会发生，比如QEMU和bochs）。
     * 所以这就是说，当IRQ#1中断产生时，我们的CPU正处在另一个ISR中。这样就会导致所有的外部中断被缓存在APIC内部的
     * FIFO队列里，进行排队等待（APIC长度为二的队列 {IRR, TMR}；参考 Intel
     * Manual Vol.3A 10.8.4）
     * 那么当ps2_process_cmd执行完后（内嵌在#APIC_TIMER_IV），CPU返回EOI给APIC，APIC紧接着将排在队里的IRQ#1发送给CPU
     * 造成误触发。也就是说，我们此时读入的scancode实则上是上一个指令的返回代码。
     *
     * Problem 1 (Fixed):
     *      但是这种方法有个问题，那就是，假若我们的某一个命令失败了一次，ps/2给出0xfe，我们重传，ps/2收到指令并给出0xfa。
     *  那么这样一来，将会由两个连续的IRQ#1产生。而APIC是最多可以缓存两个IRQ，于是我们就会漏掉一个IRQ，依然会误触发。
     * Solution:
     *      累加掩码 ;)
     *
     * Problem 2:
     *    +
     * 这种累加掩码的操作是基于只有一号IRQ产生的中断的假设，万一中间夹杂了别的中断？Race
     * Condition!
     *    +
     * 不很稳定x1，假如连续4次发送失败，那么就会导致累加的掩码上溢出，从而导致下述判断失败。
     */
#ifdef KBD_ENABLE_SPIRQ_FIX
    if ((kbd_state.state & 0xc0)) {
        kbd_state.state -= KBD_STATE_CMDPROCS;

        return;
    }
#endif

#ifdef KBD_ENABLE_SPIRQ_FIX2
    if (scancode == PS2_RESULT_ACK || scancode == PS2_RESULT_NAK) {
        return;
    }
#endif

#ifdef KBD_DBGLOG
    kprintf(KDEBUG "%x\n", scancode & 0xff);
#endif

    switch (kbd_state.state) {
        case KBD_STATE_KWAIT:
            if (scancode == 0xf0) { // release code
                kbd_state.state = KBD_STATE_KRELEASED;
            } else if (scancode == 0xe0) {
                kbd_state.state = KBD_STATE_KSPECIAL;
                kbd_state.translation_table = scancode_set2_ex;
            } else {
                key = kbd_state.translation_table[scancode];
                kbd_buffer_key_event(key, scancode, KBD_KEY_FPRESSED);
            }
            break;
        case KBD_STATE_KSPECIAL:
            if (scancode == 0x12) {
                kbd_state.state = KBD_STATE_E012;
            } else if (scancode == 0xf0) { // release code
                kbd_state.state = KBD_STATE_KRELEASED_E0;
            } else {
                key = kbd_state.translation_table[scancode];
                kbd_buffer_key_event(key, scancode, KBD_KEY_FPRESSED);

                kbd_state.state = KBD_STATE_KWAIT;
                kbd_state.translation_table = scancode_set2;
            }
            break;
        // handle the '0xE0, 0x12, 0xE0, xx' sequence
        case KBD_STATE_E012:
            if (scancode == 0xe0) {
                kbd_state.state = KBD_STATE_KSPECIAL;
                kbd_state.translation_table = scancode_set2_ex;
            }
            break;
        case KBD_STATE_KRELEASED_E0:
            if (scancode == 0x12) {
                goto escape_release;
            }
            // fall through
        case KBD_STATE_KRELEASED:
            key = kbd_state.translation_table[scancode];
            kbd_buffer_key_event(key, scancode, KBD_KEY_FRELEASED);

        escape_release:
            // reset the translation table to scancode_set2
            kbd_state.state = KBD_STATE_KWAIT;
            kbd_state.translation_table = scancode_set2;
            break;

        default:
            break;
    }
}

static uint8_t
ps2_issue_cmd(char cmd, uint16_t arg)
{
    ps2_post_cmd(PS2_PORT_CTRL_CMDREG, cmd, arg);

    // 等待PS/2控制器返回。通过轮询（polling）状态寄存器的 bit 0
    // 如置位，则表明返回代码此时就在 0x60 IO口上等待读取。
    while (!(io_inb(PS2_PORT_CTRL_STATUS) & PS2_STATUS_OFULL))
        ;

    return io_inb(PS2_PORT_ENC_CMDREG);
}

static uint8_t
ps2_issue_cmd_wretry(char cmd, uint16_t arg)
{
    uint8_t r, c = 0;
    while ((r = ps2_issue_cmd(cmd, arg)) == PS2_RESULT_NAK && c < 5) {
        c++;
    }
    if (c >= 5) {
        kprintf(KWARN "max attempt reached.\n");
    }
    return r;
}

static void
ps2_post_cmd(uint8_t port, char cmd, uint16_t arg)
{
    // 等待PS/2输入缓冲区清空，这样我们才可以写入命令
    while (io_inb(PS2_PORT_CTRL_STATUS) & PS2_STATUS_IFULL)
        ;

    io_outb(port, cmd);
    io_delay(PS2_DELAY);

    if (!(arg & PS2_NO_ARG)) {
        // 所有参数一律通过0x60传入。
        while (io_inb(PS2_PORT_CTRL_STATUS) & PS2_STATUS_IFULL)
            ;
        io_outb(PS2_PORT_ENC_CMDREG, (uint8_t)(arg & 0x00ff));
        io_delay(PS2_DELAY);
    }
}

static uint8_t
ps2_issue_dev_cmd(char cmd, uint16_t arg)
{
    ps2_post_cmd(PS2_PORT_ENC_CMDREG, cmd, arg);

    // 等待PS/2控制器返回。通过轮询（polling）状态寄存器的 bit 0
    // 如置位，则表明返回代码此时就在 0x60 IO口上等待读取。
    while (!(io_inb(PS2_PORT_CTRL_STATUS) & PS2_STATUS_OFULL))
        ;

    return io_inb(PS2_PORT_ENC_CMDREG);
}