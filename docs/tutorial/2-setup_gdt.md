## 准备工作

```sh
git checkout fedfd71f5492177a7c7d7fd2bd1529a832106395
```

观看相应视频

## 代码分析

这个commit实现了分段。

libs/libc/string下面是一些字符串操作函数，比较基础，所以略过。

主要分析boot.S和gdt.c中的代码

**为了分段，我们需要准备GDT，还要使用GDTR存放GDT的入口地址。**

`gdt.c`中负责构造好GDT。

```c
#define GDT_ENTRY 5

uint64_t _gdt[GDT_ENTRY];
uint16_t _gdt_limit = sizeof(_gdt);
```

GDT实际就是数组，一个单位（段描述符）是64bits。所以数组类型是`uint64_t`。_gdt_limit是GDT的大小。

```c
void _set_gdt_entry(uint32_t index, uint32_t base, uint32_t limit, uint32_t flags) {
    _gdt[index] = SEG_BASE_H(base) | flags | SEG_LIM_H(limit) | SEG_BASE_M(base);
    _gdt[index] <<= 32;
    _gdt[index] |= SEG_BASE_L(base) | SEG_LIM_L(limit);
}
```

`_set_gdt_entry`函数负责自动地构造GDT的一个entry（段描述符）。根据资料，参数base要分成两端，保存到56-63bits（`SEG_BASE_H(base)`）和16-39bits（`SEG_BASE_M(base)`、`SEG_BASE_L(base)`）的位置[1]。limit保存到48-51bits（`SEG_LIM_L(limit)`）的位置。index表示设置第几号描述符。参数flags稍后分析。

```c
void
_init_gdt() {
    _set_gdt_entry(0, 0, 0, 0);
    _set_gdt_entry(1, 0, 0xfffff, SEG_R0_CODE);
```

第0号entry默认为空，第1号的base为0、limit为0xfffff。这种分段模式叫做平坦模式。一个指令需要访问一个数据，地址设为x。需要先通过ds得到一个段描述符（base为0、limit为0xfffff）。因为标志位G为1，所以要在limit的20位的后面补充`fff`得到真实的地址范围上限，即x取值范围是`0x0`-`0xffffffff`。再通过0+x得到最后的地址。可以发现这个范围会映射到32位的所有内存地址。

flags为`SEG_R0_CODE`（权限为ring0的代码段）。

```c
#define SEG_R0_CODE         SD_TYPE(SEG_CODE_EXRD) | SD_CODE_DATA(1) | SD_DPL(0) | \
                            SD_PRESENT(1) | SD_AVL(0) | SD_64BITS(0) | SD_32BITS(1) | \
                            SD_4K_GRAN(1)
```

这些标志位都可以查看资料了解，`SD_DPL(0)`表示该段权限为ring0。

接下来使用GDTR存放GDT的入口地址

```assembly
.section .text
    .global start_
    .type start_, @function     /* Optional, this just give the 
                                 * linker more knowledge about the label 
                                 */
    start_:
        movl $stack_top, %esp
        /* 
            TODO: kernel init
                1. Load GDT
                2. Load IDT
                3. Enable paging
        */
        call _kernel_init
        
        subl $0x6, %esp
        movl $_gdt, 2(%esp)
        movw _gdt_limit, %ax
        movw %ax, (%esp)
        lgdt (%esp)
        addl $0x6, %esp
```

使用lgdt来设置GDTR的值

`lgdt`指令的操作数为6字节，2个低位字节为GDT的大小减1，4个高位字节为GDT的32位地址。

在`_kernel_init`调用后，`subl $0x6, %esp`抬高了栈顶，得到6字节的位置。然后把`$_gdt`、`_gdt_limit`保存到栈上。

```assembly
        movw $0x10, %cx
        movw %cx, %es
        movw %cx, %ds
        movw %cx, %fs
        movw %cx, %gs
        movw %cx, %ss
```

es等寄存器指向2号描述符（ring0数据段）

利用retf把0x8写入cs寄存器，让cs指向ring0代码段

```assembly
        pushw $0x08
        pushl $_after_gdt
        retf

    _after_gdt:
        pushl %ebx
        call _kernel_main

        cli
    j_:
        hlt
        jmp j_
```

## 参考

[1]https://wiki.osdev.org/Global_Descriptor_Table
