#include <lunaix/syscallid.h>

.section .text
    .global sigtrampoline
    sigtrampoline:
        movl %esp, %eax
        andl $0xfffffff0, %esp
        pushl %eax
        
        leal 4(%eax), %eax
        pushl %eax
        pushl $0
        pushl -4(%eax)
        call sig_dohandling

        addl $12, %esp
        
        movl $__SYSCALL_sigreturn, %eax
        popl %ebx
        int $33

    .global th_trampoline
    th_trampoline:
        movl (%esp), %eax
        movl 4(%esp), %ebx
        pushl %ebx

        calll %eax

        movl %eax, %ebx
        movl $__SYSCALL_th_exit, %eax
        int $33