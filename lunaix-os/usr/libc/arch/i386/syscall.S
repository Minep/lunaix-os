#include <lunaix/syscallid.h>

#define LUNAIX_SYSCALL 33
#define regsize 4

    .struct 8
saved_registers:
    .struct saved_registers + 5 * regsize
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
        push %ebp
        movl %esp, %ebp

        pushl %ebx
        pushl %ecx
        pushl %edx
        pushl %edi
        pushl %esi
        
        movl id(%esp), %eax
        movl a1(%esp), %ebx
        movl a2(%esp), %ecx
        movl a3(%esp), %edx
        movl a4(%esp), %edi
        movl a5(%esp), %esi
        
        int $LUNAIX_SYSCALL

        popl %esi
        popl %edi
        popl %edx
        popl %ecx
        popl %ebx

        leave
        ret