/* 高半核入口点 - 0xC0000000 */

#define __ASM__
#include <sys/abi.h>
#include <sys/failsafe.h>

.section .bss.kstack
    .global __kinit_stack_end
    
    .align 16
    __kinit_stack_end:
        .skip 2048, 0
    __kinit_stack_top:


.section .text
    .global hhk_entry_
    hhk_entry_:
        /*
            欢迎来到虚拟内存的世界！ :D
         */
        movl $__kinit_stack_top, %esp
        andl $stack_alignment, %esp

        movl $__kinit_stack_end, %eax
        movl $STACK_SANITY, (%eax)
        movl $STACK_SANITY, 4(%eax)
        movl $STACK_SANITY, 8(%eax)
        movl $STACK_SANITY, 12(%eax)

        subl $16, %esp

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
        pushl $KCODE_SEG
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