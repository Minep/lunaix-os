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

        movq $__kinit_stack_top, %rsp
        andq $stack_alignment, %rsp

        movq $__kinit_stack_end, %rax
        movl $STACK_SANITY, (%rax)
        movl $STACK_SANITY, 4(%rax)
        movl $STACK_SANITY, 8(%rax)
        movl $STACK_SANITY, 12(%rax)

        andq $stack_alignment, %rsp
        subq $16, %rsp

        # replace the temporary gdt
        call _init_gdt

        movq $_gdt, 2(%rsp)
        movw _gdt_limit, %ax
        movw %ax, (%rsp)
        lgdt (%rsp)
        
        # do a far jump to switch cs
        pushq $KCODE_SEG
        pushq $_after_gdt
        retfq

    _after_gdt:

        # initialize segment registers
        # we will not touch them again in x86_64
        movw $KDATA_SEG, %cx
        movw %cx, %es
        movw %cx, %ds
        movw %cx, %ss

        # perform arch-specific initialization before diving into kernel
        call arch_preinit

        # 加载 IDT
        movq $_idt, 2(%rsp)
        movw _idt_limit, %ax
        movw %ax, (%rsp)
        lidt (%rsp)

        /* 加载TSS段选择器 */
        movw $TSS_SEG, %ax
        ltr %ax

        call prepare_boot_handover

        xorq %rbp, %rbp
        movq %rax, %rdi
        call kernel_bootstrap

    1:
        hlt
        jmp 1b