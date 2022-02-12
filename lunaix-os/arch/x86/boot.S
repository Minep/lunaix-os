#include "multiboot.h"

.section .multiboot
    .long MB_MAGIC
    .long MB_ALIGNED_4K_MEM_MAP
    .long CHECKSUM(MB_ALIGNED_4K_MEM_MAP)

.section .bss
    /* According to System V ABI, the stack must be aligned at 16 bytes boundary */
    .align 16
    stack_bottom:
        .skip 16318, 0
    stack_top:

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

        movw $0x10, %cx
        movw %cx, %es
        movw %cx, %ds
        movw %cx, %fs
        movw %cx, %gs
        movw %cx, %ss

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