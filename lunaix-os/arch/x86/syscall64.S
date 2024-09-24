#define __ASM__
#include <lunaix/syscall.h>
#include "syscall_nr.inc"
#include "asm/variants/interrupt64.S.inc"

.section .text
    .type syscall_hndlr, @function
    .global syscall_hndlr
    syscall_hndlr:
        pushq   %rbp
        movq    %rsp, %rbp
        pushq   %rbx
        
        movq    %rdi, %rbx          // struct hart_state*

        movq    irax(%rbx), %rax          /* rax: call code as well as the return value from syscall */
        cmpq    $__SYSCALL_MAX, %rax
        jae     2f

        shlq    $3, %rax               // %rax * 8
        movabsq $syscall_table, %r8
        addq    %r8, %rax
        cmpq    $0, (%rax)
        jne     1f
    2:    
        negq    %rax
        popq    %rbx
        movq    %rbp, %rsp
        popq    %rbp

        ret

    1:
        movq    (%rax),     %rdi
        movq    irbx(%rbx), %rsi    /* rbx -> rsi #1 arg */
        movq    ircx(%rbx), %rdx    /* rcx -> rdx #2 arg */
        movq    irdx(%rbx), %rcx    /* rdx -> rcx #3 arg */
        movq    irdi(%rbx), %r8     /* rdi -> r8  #4 arg */
        movq    irsi(%rbx), %r9     /* rsi -> r9  #5 arg */
        
        call    dispatch_syscall
        
        movq    %rax, irax(%rbx)    /* save the return value */
        
        popq    %rbx
        movq    %rbp,  %rsp
        popq    %rbp
        
        ret