#define __ASM__
#include "sys/boot/multiboot.S.inc"
#include "sys/mm/mempart64.h"
#include <lunaix/compiler.h>

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
        cld
        cli

        movl $__boot_stack_top, %esp
        pushl %ebx

        # first, setup a simple initial page table
        # to enable transition to IA32e

        # L0 linkage to L1, RWX
        movl $0x3, %eax
        movl $_tmp_l1, %ebx
        orl  %eax, %ebx
        movl %ebx, (_tmp_l0)
        movl $0x80000000, %ebx
        movl %ebx, 4(_tmp_l0)

        # L1 map first 1G huge page, RWX
        orl  $(1 << 7), %eax
        movl %eax, (_tmp_l1) 
        movl $0x80000000, %eax
        movl %eax, 4(_tmp_l1)

        movl $_tmp_l0, %cr3

        # now, commencing transition

        movl %cr4, %eax
        orl  $(1 << 5), %eax    # PAE
        movl %cr4

        movl $0xc0000080, %ecx
        rdmsr
        orl $(1 << 8), %eax     # IA32_EFER.LME
        orl $(1 << 11), %eax    # IA32_EFER.NXE 
        wrmsr

        movl %cr0, %eax
        orl $(1 << 31), %eax    # PG
        movl %cr0, %eax

        jmp _ia32e

        # clear the pipeline, 
        # although cpu might already cleared for us upon mode switching
        nop
        nop
        nop
        nop

        # x86_64
    _ia32e:

        popl %ebx
        movq %rbx, %rdi
        call x86_init

        pushq $hhk_entry_
        ret