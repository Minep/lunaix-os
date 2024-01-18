## 准备工作

首先clone仓库，回滚到下面的commit。如果想编译这个版本的代码建议还是使用自行编译gcc编译器。

```sh
git checkout e0ee3d449aacd33a84cb1f58961e55f9f06acb46
```

读者还需要准备有makefile的基础

## 项目结构

- makefile：用于编译
- linker.ld：用于链接
- 其他：主要是内核代码

我们先理清楚从项目到镜像的生成过程：

1. 根据makefile中的描述编译各个子文件，得到object文件（参见build/obj文件夹里面的文件），之后对这些子文件的编译后的文件调用链接
2. 根据linker.ld对这些object文件进行链接，得到lunaix.bin
3. 使用grub-mkrescue，结合lunaix.bin来制作lunaix.iso

上面三个过程在makefile中都有体现

1.

```makefile
SOURCE_FILES := $(shell find -name "*.[cS]")
SRC := $(patsubst ./%, $(OBJECT_DIR)/%.o, $(SOURCE_FILES))

$(OBJECT_DIR):
	@mkdir -p $(OBJECT_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(ISO_DIR):
	@mkdir -p $(ISO_DIR)
	@mkdir -p $(ISO_BOOT_DIR)
	@mkdir -p $(ISO_GRUB_DIR)

$(OBJECT_DIR)/%.S.o: %.S
	@mkdir -p $(@D)
	$(CC) -c $< -o $@
......
```

2.

```makefile
$(BIN_DIR)/$(OS_BIN): $(OBJECT_DIR) $(BIN_DIR) $(SRC)
	$(CC) -T linker.ld -o $(BIN_DIR)/$(OS_BIN) $(SRC) $(LDFLAGS)
```

3.

```makefile
$(BUILD_DIR)/$(OS_ISO): $(ISO_DIR) $(BIN_DIR)/$(OS_BIN) GRUB_TEMPLATE
	@./config-grub.sh ${OS_NAME} > $(ISO_GRUB_DIR)/grub.cfg
	@cp $(BIN_DIR)/$(OS_BIN) $(ISO_BOOT_DIR)
	@grub-mkrescue -o $(BUILD_DIR)/$(OS_ISO) $(ISO_DIR)
```

## 步骤一

makefile中最开始是指定了一些目录。

```makefile
OS_ARCH := x86

BUILD_DIR := build
KERNEL_DIR := kernel
OBJECT_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
ISO_DIR := $(BUILD_DIR)/iso
ISO_BOOT_DIR := $(ISO_DIR)/boot
ISO_GRUB_DIR := $(ISO_BOOT_DIR)/grub
```

下面是把第三个参数根据第一个参数的匹配模式替换成第二个参数，这里就是通过第一个参数`%`（匹配任意字符串）匹配到`includes`最后替换成`-Iincludes`

```makefile
INCLUDES_DIR := includes
INCLUDES := $(patsubst %, -I%, $(INCLUDES_DIR))
```

不清楚的可以在下面添加打印命令，查看patsubst处理的结果

```makefile
$(OBJECT_DIR):
	@echo "================="
	@echo $(INCLUDES);
	@echo "================="
	......
```

接下来是一些名称的定义

```makefile
OS_NAME = lunaix
OS_BIN = $(OS_NAME).bin
OS_ISO = $(OS_NAME).iso

CC := i686-elf-gcc
AS := i686-elf-as

O := -O3
W := -Wall -Wextra
CFLAGS := -std=gnu99 -ffreestanding $(O) $(W)
LDFLAGS := -ffreestanding $(O) -nostdlib -lgcc
```

执行shell命令，做到所有后缀为.c或者.S文件的全部文件名词。就是搜集所有c代码文件和汇编代码文件的名称。

```makefile
SOURCE_FILES := $(shell find -name "*.[cS]")
```

同样可以修改makefile，运行`make all`来打印`SOURCE_FILES`结果，结果如下

```bash
====================
./kernel/tty/tty.c ./kernel/kernel.c ./arch/x86/boot.S
====================
```

创建上面指定的文件对应的文件夹

```makefile
$(OBJECT_DIR):
	@mkdir -p $(OBJECT_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(ISO_DIR):
	@mkdir -p $(ISO_DIR)
	@mkdir -p $(ISO_BOOT_DIR)
	@mkdir -p $(ISO_GRUB_DIR)
```

把汇编文件和c源代码文件编译成object文件，可以看到`$(INCLUDES)`作用就是指定头文件的文件夹路径

```makefile
$(OBJECT_DIR)/%.S.o: %.S
	@mkdir -p $(@D)
	$(CC) -c $< -o $@

$(OBJECT_DIR)/%.c.o: %.c 
	@mkdir -p $(@D)
	$(CC) $(INCLUDES) -c $< -o $@ $(CFLAGS)
```

## 步骤二

根据linker.ld来进行链接

```makefile
$(BIN_DIR)/$(OS_BIN): $(OBJECT_DIR) $(BIN_DIR) $(SRC)
	$(CC) -T linker.ld -o $(BIN_DIR)/$(OS_BIN) $(SRC) $(LDFLAGS)
```

下面分析一下linker.ld

```
ENTRY(start_)

SECTIONS {
    . = 0x100000;

    .text BLOCK(4K) : {
        * (.multiboot)
        * (.text)
    }

    .bss BLOCK(4K) : {
        * (COMMON)
        * (.bss)
    }

    .data BLOCK(4k) : {
        * (.data)
    }

    .rodata BLOCK(4K) : {
        * (.rodata)
    }
}
```

先是指明了入口符号是start_，这个其实是一个地址，后面会看到。

`. = 0x100000`表示起始地址为0x100000。后面.text的地址就是从0x100000开始的。随后`.bss`就是从.text结束的地址再进行对齐计算得到的地址开始的。

之后将所有object文件的节进行分配。比如把所有object文件的`.data`节的内容汇总放入到lunaix.bin的`.data`节中。COMMON代表一些未初始化的全局变量。总之lunaix.bin的节是可以自定义的，后面也会添加一些自己命名的节。`.text BLOCK(4K)`表示`.text`的地址是4K对齐的。

下面是一个object文件的部分节信息

```sh
$ readelf -S kernel.c.o 
There are 21 section headers, starting at offset 0x6fc:

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
  [ 1] .text             PROGBITS        00000000 000034 000029 00  AX  0   0  1
  [ 2] .rel.text         REL             00000000 00054c 000018 08   I 18   1  4
  [ 3] .data             PROGBITS        00000000 00005d 000000 00  WA  0   0  1
  [ 4] .bss              NOBITS          00000000 00005d 000000 00  WA  0   0  1
  [ 5] .rodata           PROGBITS        00000000 000060 000029 00   A  0   0  4
  ......
```

下面是lunaix.bin的部分节信息，可以看到结果正如在linker.ld中规划的那样。都是4K对齐的。一个页的大小也是4KB，一般是要防止两个节放入同一个页。

```sh
$ readelf -S lunaix.bin 
There are 16 section headers, starting at offset 0x3b54:

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
  [ 1] .text             PROGBITS        00100000 001000 0001e9 00  AX  0   0  1
  [ 2] .bss              NOBITS          00101000 002000 003fce 00  WA  0   0 16
  [ 3] .data             PROGBITS        00105000 002000 000004 00  WA  0   0  4
  [ 4] .rodata           PROGBITS        00106000 003000 000029 00   A  0   0  4
  ......
```

## 步骤三

制作ISO文件

```makefile
$(BUILD_DIR)/$(OS_ISO): $(ISO_DIR) $(BIN_DIR)/$(OS_BIN) GRUB_TEMPLATE
	@./config-grub.sh ${OS_NAME} > $(ISO_GRUB_DIR)/grub.cfg
	@cp $(BIN_DIR)/$(OS_BIN) $(ISO_BOOT_DIR)
	@grub-mkrescue -o $(BUILD_DIR)/$(OS_ISO) $(ISO_DIR)
```

把上面的`@`去掉，可以知道执行了什么命令。

```
./config-grub.sh lunaix > build/iso/boot/grub/grub.cfg
cp build/bin/lunaix.bin build/iso/boot
grub-mkrescue -o build/lunaix.iso build/iso
```

先是执行下面脚本，参数$1就是lunaix，结果重定向到build/iso/boot/grub/grub.cfg文件。

```sh
#!/usr/bin/bash

export _OS_NAME=$1

echo $(cat GRUB_TEMPLATE | envsubst)
```

尝试运行下面命令，可以知道我们要提供`$_OS_NAME`的值。那么上面的第一行就是用于提供值$1，也就是命令行的参数lunaix。`envsubset`会把`$_OS_NAME`的值替换成lunaix。

```sh
$ cat GRUB_TEMPLATE
menuentry "$_OS_NAME" {
	multiboot /boot/$_OS_NAME.bin
}
```

build/iso/boot/grub/grub.cfg文件内容和预期一样。在multiboot后面指定bin的路径即可。这个grub.cfg也是可以自定义的。

```
menuentry "lunaix" { multiboot /boot/lunaix.bin }
```

grub-mkrescue会根据grub.cfg来制作ISO文件。制作后会放入自动生成的bootloader，所以我们不需要写bootloader。只需要从入口点开始写代码。

大概框架就是这样，具体细节之后会学习到。

## 内核代码分析

### arch/x86/boot.S

先看.text节的内容

```assembly
.section .text
    .global start_
    .type start_, @function
    start_:
        movl $stack_top, %esp
        /* 
            TODO: kernel init
                1. Load GDT
                2. Load IDT
                3. Enable paging
        */
        call _kernel_init

        pushl %ebx
        call _kernel_main

        cli
    j_:
        hlt
        jmp j_
```

start_就是链接文件里面提到的ENTRY，引导程序会引导到这个指定的入口。

伪指令.global声明_start为全局符号。

如果其他文件里面要jump到这个_start，链接时会从全局符号里面看是否存在这个符号，如果存在，则使用全局符号的地址。总之，如果要在其他文件使用这个文件的函数，需要申明成全局的。_

`.type start_, @function`申明为函数。

里面简单的初始化了esp栈顶，调用了\_kernel_init和_kernel_main。

\_kernel_init还没有代码，_kernel_main用于打印信息。

最后就是一个死循环，防止退出。

之后来看看.multiboot节

```assembly
.section .multiboot
    .long MB_MAGIC
    .long MB_ALIGNED_4K_MEM_MAP
    .long CHECKSUM(MB_ALIGNED_4K_MEM_MAP)
```

第一个.long表示在节的第一个32bits中，存储MB_MAGIC（0x1BADB002）。第三个用于配置一些选项（0x3表示在页面边界加载模块和提供内存地图）。第三个用于校验。

其他类推，可以打开二进制编辑器来验证。这个节的作用就是为了满足约定。为了让GRUB 能够识别镜像文件，我们需要硬编码。最好像链接脚本那样把.multiboot放到第一个位置。

### kernel/tty/tty.c

根据视频中提到的文档定义宽度和长度，表示每行80个字符，总共25行（VGA文本模式）。buffer指向的是一个固定的地址，也是文档定义的。操作这个地址才能在屏幕上打印出字符。最后是两个表示当前位置的全局变量。

```c
#define TTY_WIDTH 80
#define TTY_HEIGHT 25

vga_atrributes *buffer = 0xB8000;

vga_atrributes theme_color = VGA_COLOR_BLACK;

uint32_t TTY_COLUMN = 0;
uint16_t TTY_ROW = 0;
```

根据文档设置background color和foreground color。unsigned short两个字节，bg和fg占高8位（设置颜色），低八位（一个字节）用于存储字符信息。

```c
typedef unsigned short vga_atrributes; 

void tty_set_theme(vga_atrributes fg, vga_atrributes bg) {
    theme_color = (bg << 4 | fg) << 8;
}
```

`tty_put_char`实现了字符打印。如果输入字符是`\n`则把行数加1,如果是`\r`则把光标移动到行头部。

```c
void tty_put_char(char chr) {
    if (chr == '\n') {
        TTY_COLUMN = 0;
        TTY_ROW++;
    }
    else if (chr == '\r') {
        TTY_COLUMN = 0;
    }
    else {
        *(buffer + TTY_COLUMN + TTY_ROW * TTY_WIDTH) = (theme_color | chr);
        TTY_COLUMN++;
        if (TTY_COLUMN >= TTY_WIDTH) {
            TTY_COLUMN = 0;
            TTY_ROW++;
        }
    }

    if (TTY_ROW >= TTY_HEIGHT) {
        tty_scroll_up();
        TTY_ROW--;
    } 
}
```

下面是真正写入字符的一行语句，高八位是颜色信息，低八位是`chr`。

```c
*(buffer + TTY_COLUMN + TTY_ROW * TTY_WIDTH) = (theme_color | chr);
```

`tty_scroll_up`在屏幕满的时候被调用，用于滚动屏幕，该函数暂时未实现。其他情况也是很容易看懂的。

## 参考

https://wiki.osdev.org/User:Zesterer/Bare_Bones#kernel.c
