## 准备工作

```sh
git checkout fedfd71f5492177a7c7d7fd2bd1529a832106395
```

观看相应视频

## 代码分析

软件或者硬件中断发生后，CPU会通过一个表去寻找一个函数指针，并调用。那么我们需要弄清楚怎么找怎么调用。

比如我们使用`int $0`（软件中断）会触发0号异常，CPU会根据IDTR来寻找IDT。最后根据IDT的第0项（不同于0号GDT为空）并把相关中断信息保存到栈上，最后调用指定函数。

IDT也是由8字节，也是通过IDTR来定位。boot.S新增了下面这段来初始化IDTR。和GDTR差不多，略过。

```assembly
        movl $_idt, 2(%esp)
        movw _idt_limit, %ax
        movw %ax, (%esp)
        lidt (%esp)
```

接下来看看如何设置IDT的entry。

```c
void _set_idt_entry(uint32_t vector, uint16_t seg_selector, void (*isr)(), uint8_t dpl) {
    uintptr_t offset = (uintptr_t)isr;
    _idt[vector] = (offset & 0xffff0000) | IDT_ATTR(dpl);
    _idt[vector] <<= 32;
    _idt[vector] |= (seg_selector << 16) | (offset & 0x0000ffff);
}
```

把段选择子（段寄存器存储的值）保存到16-31bits的位置，因为我们存的相当于函数指针，所以段选择子的值是CS段寄存器保存的值（指向代码段）。分段中设置了CS值为0x8。函数指针分成两个部分保存，48-63bits位置保存高16位，0-15bits保存低16位。根据Abort、Trap、Fault类型来设置8-11位。最后设置一下剩余的标志位即可。

下面是已经安装了一个0号异常处理函数

```c
void
_init_idt() {
    _set_idt_entry(FAULT_DIVISION_ERROR, 0x08, _asm_isr0, 0);
}
```

`_asm_isr0`实现在`arch/x86/interrupt.S`中下面代码

```assembly
.section .text
    isr_template 0
```

这里用到了一个伪指令`.macro`，起到宏的作用。

```assembly
.macro isr_template vector, no_error_code=1
    .global _asm_isr\vector
    .type _asm_isr\vector, @function
    _asm_isr\vector:
        .if \no_error_code
            pushl $0x0
        .endif
        pushl $\vector
        jmp interrupt_wrapper
.endm
```

vector这里是0，那么`_asm_isr\vector`就是`_asm_isr0`。`no_error_code=1`表示默认值为1。

最后可以转换成下面代码。

```assembly
    .global _asm_isr0
    .type _asm_isr0, @function
    _asm_isr0:
        .if 1
            pushl $0x0
        .endif
        pushl 0
        jmp interrupt_wrapper
```

这里是处理异常的部分，执行到了这里，CPU会把EFLAGS、CS、EIP保持在栈上给我们处理。也可能多push一个error code。这段宏的作用是根据是否需要手动push error code来定义函数，从而我们可以规范化这个结构，统一使用结构体`isr_param`来描述。`#pragma pack(push, 1)`和`#pragma pack(pop)`表示不对该结构体进行对齐。因为CPU给我们提供的数据也没有进行对齐处理。

```c
#pragma pack(push, 1)
typedef struct {
    unsigned int vector;
    unsigned int err_code;
    unsigned int eip;
    unsigned short cs;
    unsigned int eflags;
} isr_param;
#pragma pack(pop)
```

因为栈没有对齐，而且调用函数前要先对齐栈，所以先要向上对齐，`movl %eax, (%esp)`把旧栈顶保存起来，调用`interrupt_handler`。

```assembly
    interrupt_wrapper:

        movl %esp, %eax
        andl $0xfffffff0, %esp
        subl $16, %esp
        movl %eax, (%esp)

        call interrupt_handler
        pop %eax
        movl %eax, %esp
        addl $8, %esp

        iret
```

调用完后，恢复旧栈顶，再恢复成未对齐的状态，执行iret（需要error code在栈上）。所以push error code是必要的。

因为栈顶保存的是旧栈顶的值，所以参数param指向旧栈顶。旧栈顶的值正好和param的字段一一对应。根据vector来分配调用指定函数。

```c
void 
interrupt_handler(isr_param* param) {
    switch (param->vector)
    {
        case 0:
            isr0(param);
            break;
    }
}
```

isr0的功能就是清屏并打印信息。

```c
void isr0 (isr_param* param) {
    tty_clear();
    tty_put_str("!!PANIC!!");
}
```

## FAQ

**1.为什么本应该push error code的情况却没有push？**

int指令主动触发的中断不会push error code。

> If INT n provides a vector for one of the architecturally-defined exceptions, the processor generates an interrupt to the correct vector (to access the exception handler) but does not push an error code on the stack.[1]

下面这种被动触发会push error code。

```c
int a = 1/0;
```

**2.为什么有时候无法追踪异常来源？**

触发了Abort。之后我们会写一个打印内核调用栈的功能，触发Abort后，如果试图打印内核函数调用栈来搜索问题代码，最后会得到不可信的结果（大概率会访问未映射的内存，又触发General Protection）。

## 参考

[1]Intel手册，Volume 3，6.4.2 Software-Generated Exceptions
