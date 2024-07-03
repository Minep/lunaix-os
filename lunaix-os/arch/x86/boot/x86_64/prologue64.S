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
        movq $__kinit_stack_top, %rsp
        andq $stack_alignment, %rsp

        movq $__kinit_stack_end, %rax
        movq $STACK_SANITY, (%rax)
        movq $STACK_SANITY, 4(%rax)
        movq $STACK_SANITY, 8(%rax)
        movq $STACK_SANITY, 12(%rax)

        subq $16, %rsp

        /* 
            加载 GDT 
            P.s. 虽然GDT在分页后已变得不重要，甚至可以忽略不作。但为了保持完整性，还是选择加载他
                    这主要是为了保险起见，让GDTR有一个合法的值，否则多咱的粗心大意，容易出#GP
        */
        call _init_gdt

        movq $_gdt, 2(%rsp)
        movw _gdt_limit, %ax
        movw %ax, (%esp)
        lgdt (%esp)

        # initialize segment registers
        # we will not touch them again in x86_64
        movw $KDATA_SEG, %cx
        movw %cx, %es
        movw %cx, %ds
        movw %cx, %fs
        movw %cx, %gs
        movw %cx, %ss
        
        # do a far jump to switch cs
        pushw $KCODE_SEG
        pushq $_after_gdt
        retf

    _after_gdt:

        # 加载 IDT
        movq $_idt, 2(%rsp)
        movw _idt_limit, %ax
        movw %ax, (%rsp)
        lidt (%rsp)

        # perform arch-specific initialization before diving into kernel
        call arch_preinit

        /* 加载TSS段选择器 */
        movw $TSS_SEG, %ax
        ltr %ax

        movq $bhctx_buffer, %rdi       # mb_parser.c
        call kernel_bootstrap

    1:
        hlt
        jmp 1b