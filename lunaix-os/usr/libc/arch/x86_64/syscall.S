#include <lunaix/syscallid.h>

#define LUNAIX_SYSCALL 33
#define regsize 8

    .struct  2 * regsize # rip, rbp
id:
    .struct id + regsize
a1:
    .struct a1 + regsize
a2:
    .struct a2 + regsize
a3:
    .struct a3 + regsize
a4:
    .struct a4 + regsize
a5:

.section .text
    .type do_lunaix_syscall, @function    
    .global do_lunaix_syscall

    do_lunaix_syscall:
        pushq %rbp
        movq %rsp, %rbp

        pushq %rbx

        movq %rcx, %r10
       
        movq %rdi, %rax
        movq %rsi, %rbx
        movq %rdx, %rcx
        movq %r10, %rdx
        movq %r8, %rdi
        movq %r9, %rsi
        
        int $LUNAIX_SYSCALL

        popq %rbx

        leave
        ret