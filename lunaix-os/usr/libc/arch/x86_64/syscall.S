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

    // do_lunaix_syscall always called with parameters
    //  passed via stack
    do_lunaix_syscall:
        pushq %rbp
        movq %rsp, %rbp

        pushq %rbx
       
        movq id(%rbp), %rax
        movq a1(%rbp), %rbx
        movq a2(%rbp), %rcx
        movq a3(%rbp), %rdx
        movq a4(%rbp), %rdi
        movq a5(%rbp), %rsi
        
        int $LUNAIX_SYSCALL

        popq %rbx

        leave
        ret