#include <lunaix/syscallid.h>

#define LUNAIX_SYSCALL 33
#define regsize 4

    .struct  2 * regsize   # eip, ebp
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
        pushl %edi
        pushl %esi

        movl id(%ebp), %eax
        movl a1(%ebp), %ebx
        movl a2(%ebp), %ecx
        movl a3(%ebp), %edx
        movl a4(%ebp), %edi
        movl a5(%ebp), %esi
        
        int $LUNAIX_SYSCALL

        popl %esi
        popl %edi
        popl %ebx

        leave
        ret