## 准备工作

```
git checkout af336b49c908dc0d2b62846a19001d4dac7cad61
```

观看对应视频。

## 代码分析

### mutex

```c
#include <stdatomic.h>

struct sem_t {
    _Atomic unsigned int counter;
    // FUTURE: might need a waiting list
};
```

`stdatomic.h`里面的函数是可以使用的。我们会用到里面的原子操作。

counter的类型是`unsigned int`的原子版本（_Atomic）

如果sem->counter为0的话，就会一直等待。

```c
void sem_wait(struct sem_t *sem) {
    while (!atomic_load(&sem->counter)) {
        // TODO: yield the cpu
    }
    atomic_fetch_sub(&sem->counter, 1);
}
```

增加变量的值

```c
void sem_post(struct sem_t *sem) {
    atomic_fetch_add(&sem->counter, 1);
    // TODO: wake up a thread
}
```

mutex_lock需要获得锁，如果没有（sem->counter为0）就要等待。mutex_unlock会释放锁，sem->counter自增1。

```c
typedef struct sem_t mutex_t;

static inline void mutex_init(mutex_t *mutex) {
    sem_init(mutex, 1);
}

static inline unsigned int mutex_on_hold(mutex_t *mutex) {
    return !atomic_load(&mutex->counter);
}

static inline void mutex_lock(mutex_t *mutex) {
    sem_wait(mutex);
}

static inline void mutex_unlock(mutex_t *mutex) {
    sem_post(mutex);
}
```

### kernel/peripheral/ps2kbd.c

`ps2_post_cmd`用于向端口写入命令

`io_inb(PS2_PORT_CTRL_STATUS)`读取端口，读取后才能清空值。

`PS2_PORT_CTRL_STATUS`代表Status Register，它在0x64。

> Data should be written to the
> controller's input buffer only if the input buffer's full bit in the
> status register is equal to 0.[1]

full bit就是第二位bit，如果输入缓冲区满，该bit为1

> When the controller reads the input buffer, this bit will return to 0.[2]

这里就是等待输入buffer被读取。如果没有被读取，就会阻塞在while。参数可以稍后再看。

```c
static void ps2_post_cmd(uint8_t port, char cmd, uint16_t arg) {
    char result;
    // 等待PS/2输入缓冲区清空，这样我们才可以写入命令
    while((result = io_inb(PS2_PORT_CTRL_STATUS)) & PS2_STATUS_IFULL);

    io_outb(port, cmd);
    io_delay(PS2_DELAY);
    
    if (!(arg & PS2_NO_ARG)) {
        // 所有参数一律通过0x60传入。
        io_outb(PS2_PORT_ENC_CMDREG, (uint8_t)(arg & 0x00ff));
        io_delay(PS2_DELAY);
    }
}
```

因为写入端口需要时间，所以要使用`io_delay`。`io_delay`是用一个循环来实现的。

```c
static inline void
io_delay(int counter)
{
    asm volatile (
        "   test %0, %0\n"
        "   jz 1f\n"
        "2: dec %0\n"
        "   jnz 2b\n"
        "1: dec %0"::"a"(counter));
}
```

`ps2_issue_dev_cmd`调用`ps2_post_cmd`，最后要等待状态。之前等待的是`PS2_STATUS_IFULL`，这次等待的是`PS2_STATUS_OFULL`。这里循环里面进行了取反，表示output未满就等待。

> The output buffer should be read only when the output buffer's full bit in the status register is 1.[3]

```c
static uint8_t ps2_issue_dev_cmd(char cmd, uint16_t arg) {
    ps2_post_cmd(PS2_PORT_ENC_CMDREG, cmd, arg);

    char result;
    
    // 等待PS/2控制器返回。通过轮询（polling）状态寄存器的 bit 0
    // 如置位，则表明返回代码此时就在 0x60 IO口上等待读取。
    while(!((result = io_inb(PS2_PORT_CTRL_STATUS)) & PS2_STATUS_OFULL));

    return io_inb(PS2_PORT_ENC_CMDREG);
}
```

`ps2_kbd_init`函数可以看注释。该函数中的下面两个函数需要接着分析。

```c
    intr_subscribe(PC_KBD_IV, intr_ps2_kbd_handler);
    timer_run_ms(5, ps2_process_cmd, NULL, TIMER_MODE_PERIODIC);
```

`intr_ps2_kbd_handler`在键盘中断（键盘按下或抬起时触发）时被调用。该函数会对键盘发出的扫描码进行预处理得到`Lunaix Keycode`，保存到`kbd_keycode_t`中。

每5毫秒会执行一次`ps2_process_cmd`，来进一步处理。

### ps2_kbd_init

用于初始化，略。

### kbd_keycode_t

根据资料，数字1的扫描码是0x16，所以`scancode_set2[0x16]`是`KEY_NUM(1)`。即数组下标是扫描码，值为lunaix-os自定义的*Lunaix Keycode*。如果不想查资料，可以在中断处理函数中打印接收的扫描码。

```c
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
```

对于字符1，它的category是0x0，sequence是0x31（1的ASCII值）。KEY_NUM(1)的值就是这么来的。

```c
//      Lunaix Keycode
//       15        7         0
// key = |xxxx xxxx|xxxx xxxx|
// key[0:7] = sequence
// key[8:15] = category
//   0x0: ASCII codes
//   0x1: keypad keys
//   0x2: Function keys
//   0x3: Cursor keys (arrow keys)
//   0x4: Modifier keys
//   0xff: Other keys (Un-categorized)
```

再举一个例子，`KEY_F8`属于`Function keys`，`sequence`是0x07（这个值也是自定义的）。它的值是`(0x07 | 0x0200)`。

可以理解成，Lunaix Keycode从两个维度来分类，一个是类型，一个是自定义序号。

### intr_ps2_kbd_handler

里面有一些对`kbd_state.state`的操作，用于切换视频中提到的状态机的状态。

读取扫描码。

```c
	// Do not move this line. It is in the right place and right order.
    // This is to ensure we've cleared the output buffer everytime, so it won't pile up across irqs.
    uint8_t scancode = io_inb(PS2_PORT_ENC_DATA);
    kbd_keycode_t key;
```

我们读取的不一定是scancode，还有可能是指令的返回码0xfa。对于这种数据不能用intr_ps2_kbd_handler处理。这个稍后来看。

先来看看的一个case。如果`scancode == 0xf0`，说明释放了一个按键。状态改为`KBD_STATE_KRELEASED`。如果`scancode == 0xe0`，则使用特殊表`scancode_set2_ex`。如果都不是，就可以正常地获得Lunaix Keycode。

```c
switch (kbd_state.state)
    {
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
```

第二个case。在特殊处理的状态下，如果没有释放，就正常地获得Lunaix Keycode。最后换成普通的表。

```c
    case KBD_STATE_KSPECIAL:
        if (scancode == 0xf0) { //release code
            kbd_state.state = KBD_STATE_KRELEASED;
        } else {
            key = kbd_state.translation_table[scancode];
            kbd_buffer_key_event(key, scancode, KBD_KEY_FPRESSED);

            kbd_state.state = KBD_STATE_KWAIT;
            kbd_state.translation_table = scancode_set2;
        }
        break;
```

最后一个case略。

### kbd_buffer_key_event

state完整位图布局如下

```tex
15-10 保留
9 右ALT
8 左ALT
7 右CTRL
6 左CTRL
5 右SHIFT
4 左SHIFT
3 CAPSLOCK
2 NUMLOCK
1 SCREENLOCK
0 此位为1（KBD_KEY_FPRESSED）表示按下，为0表示抬起
```

这里是用来记录是否CAPSLOCK等键处于按下状态。简单来说下面代码是用于设置state的一个位。因为我们的状态是用一个bit表示的，所以处理起来有些麻烦。如果用一个byte表示一个状态，可以直接用更简单的异或来切换。

```c
    if (key == KEY_CAPSLK) {
        kbd_state.key_state ^= KBD_KEY_FCAPSLKED & -state;
    } else if (key == KEY_NUMSLK) {
        kbd_state.key_state ^= KBD_KEY_FNUMBLKED & -state;
    } else if (key == KEY_SCRLLK) {
        kbd_state.key_state ^= KBD_KEY_FSCRLLKED & -state;
    } 
```

state的值是0或者1。-state就是0x0或者0xffff。

假如key == KEY_CAPSLK，而且state是1，kbd_state.key_state就会异或上0x8。

假如key == KEY_CAPSLK，而且state是0，kbd_state.key_state就会异或上0。kbd_state.key_state不会改变

所以按下CAPSLOCK再后抬起，kbd_state.key_state还是会CAPSLOCK锁定的状态。只有再次按下CAPSLOCK才会接触锁定。

lunaix定义modifier的sequence刚好是每个modifier state bit相对于lshift的位移。这里的位移就是于根据key设置对应的modifier state。当按下lctrl，`key & 0xff == 2`，lshift 左移两位刚好等于lctrl state bit的位置，后面的state取补就是决定这个bit该不该被设置（如果release，就不用设置了）。

```c
} else {
        if ((key & MODIFR)) {
            kbd_kstate_t tmp = (KBD_KEY_FLSHIFT_HELD << (key & 0x00ff));
            kbd_state.key_state = (kbd_state.key_state & ~tmp) | (tmp & -state);
        }
```

如果按下了shift，就用`scancode_set2_shift`表。

```c
        else if (!(key & 0xff00) && (kbd_state.key_state & (KBD_KEY_FLSHIFT_HELD | KBD_KEY_FRSHIFT_HELD))) {
            key = scancode_set2_shift[scancode];
        }
        state = state | kbd_state.key_state;
        key = key & (0xffdf | -('a' > key || key > 'z' || !(state & KBD_KEY_FCAPSLKED)));
```

最后得到预处理好的key，存储到keyevent_pkt（）。

最后是让键盘亮灯的操作，但是这个操作会产生返回码干扰我们的状态机。

```c
    if (state & KBD_KEY_FPRESSED) {
        // Ooops, this guy generates irq!
        ps2_device_post_cmd(PS2_KBD_CMD_SETLED, (kbd_state.key_state >> 1) & 0x00ff);
    }
```

所以`intr_ps2_kbd_handler`中代码使用了叠加掩码的方式来保护状态机

```c
#ifdef KBD_ENABLE_SPIRQ_FIX
    if ((kbd_state.state & 0xc0)) {
        kbd_state.state -= KBD_STATE_CMDPROCS;

        return;
    }
#endif
```

`ps2_device_post_cmd`略

### 接收key

kernel/lxinit.c调用了`kbd_recv_key`。直接打印了keyevent.keycode的最低字节。

```c
    struct kdb_keyinfo_pkt keyevent;
    while (1) {
        if (!kbd_recv_key(&keyevent)) {
            // yield();
            continue;
        }
        if ((keyevent.state & KBD_KEY_FPRESSED) &&
            (keyevent.keycode & 0xff00) <= KEYPAD) {
            tty_put_char((char)(keyevent.keycode & 0x00ff));
            tty_sync_cursor();
        }
    }
```

`kbd_buffer_key_event`中存储的`kdb_keyinfo_pkt`会在`kbd_recv_key`中读取。视频讲过它的原理，而且比较简单，所以略过。

```c
int kbd_recv_key(struct kdb_keyinfo_pkt* key_event) {
    if (!key_buf.buffered_len) {
        return 0;
    }
    mutex_lock(&key_buf.mutex);

    struct kdb_keyinfo_pkt* pkt_current = &key_buf.buffer[key_buf.read_ptr];

    *key_event = *pkt_current;
    key_buf.buffered_len--;
    key_buf.read_ptr = (key_buf.read_ptr + 1) % PS2_KBD_RECV_BUFFER_SIZE;

    mutex_unlock(&key_buf.mutex);
    return 1;
}
```

## 参考

[1]IBM_PC_AT_Technical_Reference_Mar84, 1-40 System Board, Input Buffer

[2]IBM_PC_AT_Technical_Reference_Mar84, 1-38 System Board, Status-Register Bit Definition, Bit 1

[3]IBM_PC_AT_Technical_Reference_Mar84, 1-40 System Board, Output Buffer
