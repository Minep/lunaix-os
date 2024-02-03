#include <lunaix/syscallid.h>

#define LUNAIX_SYSCALL 33

.section .text
    .type do_lunaix_syscall, @function
    .global do_lunaix_syscall
    do_lunaix_syscall:
        push %ebp
        movl %esp, %ebp
        
        addl $8, %esp
        popl %eax
        popl %ebx
        popl %ecx
        popl %edx
        popl %edi
        popl %esi
        
        movl %ebp, %esp

        int $LUNAIX_SYSCALL

        leave
        ret