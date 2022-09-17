#ifndef __LUNAIX_PS2KBD_H
#define __LUNAIX_PS2KBD_H

#include <hal/io.h>
#include <lunaix/ds/mutex.h>
#include <lunaix/keyboard.h>

#define PS2_PORT_ENC_DATA 0x60
#define PS2_PORT_ENC_CMDREG 0x60
#define PS2_PORT_CTRL_STATUS 0x64
#define PS2_PORT_CTRL_CMDREG 0x64

#define PS2_STATUS_OFULL 0x1
#define PS2_STATUS_IFULL 0x2

#define PS2_RESULT_ACK 0xfa
#define PS2_RESULT_NAK 0xfe // resend
#define PS2_RESULT_ECHO 0xee
#define PS2_RESULT_TEST_OK 0x55

// PS/2 keyboard device related commands
#define PS2_KBD_CMD_SETLED 0xed
#define PS2_KBD_CMD_ECHO 0xee
#define PS2_KBD_CMD_SCANCODE_SET 0xf0
#define PS2_KBD_CMD_IDENTIFY 0xf2
#define PS2_KBD_CMD_SCAN_ENABLE 0xf4
#define PS2_KBD_CMD_SCAN_DISABLE 0xf5

// PS/2 *controller* related commands
#define PS2_CMD_PORT1_DISABLE 0xad
#define PS2_CMD_PORT1_ENABLE 0xae
#define PS2_CMD_PORT2_DISABLE 0xa7
#define PS2_CMD_PORT2_ENABLE 0xa8
#define PS2_CMD_SELFTEST 0xaa
#define PS2_CMD_SELFTEST_PORT1 0xab

#define PS2_CMD_READ_CFG 0x20
#define PS2_CMD_WRITE_CFG 0x60

#define PS2_CFG_P1INT 0x1
#define PS2_CFG_P2INT 0x2
#define PS2_CFG_TRANSLATION 0x40

#define PS2_DELAY 1000

#define PS2_CMD_QUEUE_SIZE 8

#define PS2_NO_ARG 0xff00

struct ps2_cmd
{
    char cmd;
    char arg;
};

struct ps2_kbd_state
{
    volatile char state;
    volatile char masked;
    volatile kbd_kstate_t key_state;
    kbd_keycode_t* translation_table;
};

struct ps2_cmd_queue
{
    struct ps2_cmd cmd_queue[PS2_CMD_QUEUE_SIZE];
    int queue_ptr;
    int queue_len;
    mutex_t mutex;
};

/**
 * @brief 向PS/2控制器的控制端口(0x64)发送指令并等待返回代码。
 * 注意，对于没有返回代码的命令请使用`ps2_post_cmd`，否则会造成死锁。
 * 通过调用该方法向控制器发送指令，请区别 ps2_issue_dev_cmd
 *
 * @param cmd
 * @param args
 */
static uint8_t
ps2_issue_cmd(char cmd, uint16_t arg);

/**
 * @brief 向PS/2控制器的编码器端口(0x60)发送指令并等待返回代码。
 * 注意，对于没有返回代码的命令请使用`ps2_post_cmd`，否则会造成死锁。
 * 通过调用该方法向PS/2设备发送指令，请区别 ps2_issue_cmd
 *
 * @param cmd
 * @param args
 */
static uint8_t
ps2_issue_dev_cmd(char cmd, uint16_t arg);

/**
 * @brief 向PS/2控制器发送指令，不等待返回代码。
 *
 * @param port 端口号
 * @param cmd
 * @param args
 * @return char
 */
static void
ps2_post_cmd(uint8_t port, char cmd, uint16_t arg);

void
ps2_device_post_cmd(char cmd, char arg);

void
ps2_kbd_init();

void
ps2_process_cmd(void* arg);

#endif /* __LUNAIX_PS2KBD_H */
