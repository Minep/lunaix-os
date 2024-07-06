#define __ASM__

#if defined(CONFIG_X86_BL_MB) || defined(CONFIG_X86_BL_MB2)
#include "sys/boot/multiboot.S.inc"
#endif

#include "sys/mm/mempart64.h"

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
        movl $0x80000000, %ebx
        movl %ebx, _tmp_l0 + 4

        # L1 map first 1G huge page, RWX
        orl  $(1 << 7), %eax
        movl %eax, _tmp_l1
        movl $0x80000000, %eax
        movl %eax, _tmp_l1 + 4

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
        movl %cr0, %eax

        jmp _ia32e

        # clear the pipeline, 
        # although cpu might already cleared for us upon mode switching
        .nop
        .nop
        .nop
        .nop

        # x86_64
    _ia32e:

        .code64
        popq %rbx
        movq %rbx, %rdi
        call x86_init

        movabsq $hhk_entry_, %rax
        pushq %rax 
        retq