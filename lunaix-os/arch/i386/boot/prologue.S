/* 高半核入口点 - 0xC0000000 */

#define __ASM__
#include <sys/abi.h>

.section .bss
    .align 16
    .skip 2048, 0
    __kinit_stack_top:
    # TODO 
    # This stack was too small that corrupt the ambient kernel structures.
    #  (took me days to figure this out!)
    # We should spent more time to implement a good strategy to detect such
    #  run-over (we can check these invariants when we trapped in some non-recoverable
    #  state and provide good feedback to user)

.section .text
    .global hhk_entry_
    hhk_entry_:
        /*
            欢迎来到虚拟内存的世界！ :D
         */
        movl $__kinit_stack_top, %esp
        andl $stack_alignment, %esp
        subl $16, %esp
        /* 
            最终还是决定将IDT&GDT的初始化和安装放在这里
            注意：由于已开启分页，GDTR与IDTR里面放的不是物理地址，是线性地址！ 
        */
        /* 
            加载 GDT 
            P.s. 虽然GDT在分页后已变得不重要，甚至可以忽略不作。但为了保持完整性，还是选择加载他
                    这主要是为了保险起见，让GDTR有一个合法的值，否则多咱的粗心大意，容易出#GP
        */
        call _init_gdt

        movl $_gdt, 2(%esp)
        movw _gdt_limit, %ax
        movw %ax, (%esp)
        lgdt (%esp)

        /* 更新段寄存器 */
        movw $KDATA_SEG, %cx
        movw %cx, %es
        movw %cx, %ds
        movw %cx, %fs
        movw %cx, %gs
        movw %cx, %ss
        
        /* 更新 CS:EIP */
        pushw $KCODE_SEG
        pushl $_after_gdt
        retf

    _after_gdt:
        subl $16, %esp

        # 加载 IDT
        movl $_idt, 2(%esp)
        movw _idt_limit, %ax
        movw %ax, (%esp)
        lidt (%esp)

        # perform arch-specific initialization before diving into kernel
        call arch_preinit

        /* 加载TSS段选择器 */
        movw $TSS_SEG, %ax
        ltr %ax

        movl $bhctx_buffer, (%esp)       # mb_parser.c
        call kernel_bootstrap

    1:
        hlt
        jmp 1b