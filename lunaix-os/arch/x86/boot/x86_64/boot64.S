#define __ASM__

#if defined(CONFIG_X86_BL_MB) || defined(CONFIG_X86_BL_MB2)
#include "sys/boot/multiboot.S.inc"
#endif

#include "sys/mm/mempart64.h"

.section .boot.bss
    .align 8
    __tmp_gdt:
        .long 0x0
        .long 0x0
        .long 0x0000ffff
        .long 0x00ef9a00
        .long 0x0000ffff
        .long 0x00cf9200

.section .boot.bss

    .align 16
        .skip 512, 0
    __boot_stack_top:

.section .boot.bss

    .align 4096
    _tmp_l0:
        .skip 4096
    _tmp_l1:
        .skip 4096

.section .boot.text
    .global start_
    .type start_, @function

    start_: 
        .code32
        cld
        cli

        movl $__boot_stack_top, %esp
        pushl $0
        pushl %ebx

        # first, setup a simple initial page table
        # to enable transition to IA32e

        # L0 linkage to L1, RWX
        movl $0x3, %eax
        movl $_tmp_l1, %ebx
        orl  %eax, %ebx
        movl %ebx, _tmp_l0
        xorl %ebx, %ebx
        movl %ebx, _tmp_l0 + 4
        
        # Entry 0
        orl  $(1 << 7), %eax
        movl %eax, _tmp_l1
        movl %ebx, _tmp_l1 + 4

        # Entry 1
        addl $0x40000000, %eax
        movl %eax, _tmp_l1 + 8
        movl %ebx, _tmp_l1 + 12

        # Entry 2
        addl $0x40000000, %eax
        movl %eax, _tmp_l1 + 16
        movl %ebx, _tmp_l1 + 20

        # Entry 3
        addl $0x40000000, %eax
        movl %eax, _tmp_l1 + 24
        movl %ebx, _tmp_l1 + 28

        movl $_tmp_l0, %eax
        movl %eax, %cr3

        # now, commencing transition

        movl %cr4, %eax
        orl  $(1 << 5), %eax    # PAE
        movl %eax, %cr4

        movl $0xc0000080, %ecx
        rdmsr
        orl $(1 << 8), %eax     # IA32_EFER.LME
        orl $(1 << 11), %eax    # IA32_EFER.NXE 
        wrmsr

        movl %cr0, %eax
        orl $(1 << 31), %eax    # PG
        movl %eax, %cr0

        jmp _ia32e_compat

        # clear the pipeline, 
        # although cpu might already cleared for us upon mode switching
        .nop
        .nop
        .nop
        .nop

        # x86_64 compatibility mode
        # load a temporary gdt for getting into long mode
    _ia32e_compat:
        .code32
        subl $16, %esp
        movl $__tmp_gdt, 2(%esp)
        movw $23, (%esp)
        lgdtl (%esp)

        addl $16, %esp
        
        # do a far jump to switch cs
        pushl $0x08
        pushl $_ia32e
        retf
    
    _ia32e:
        .code64
        movw $0x10, %cx
        movw %cx, %ds
        movw %cx, %ss

        popq %rbx
        movq %rbx, %rdi
        call x86_init

        movabsq $hhk_entry_, %rax
        pushq %rax 
        retq