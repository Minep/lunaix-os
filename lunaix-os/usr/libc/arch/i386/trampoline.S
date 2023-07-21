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