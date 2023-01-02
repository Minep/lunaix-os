#define __ASM__
#include <lunaix/syscall.h>

.section .text
    .global _u_start
    _u_start:
        movl $2, %eax
        call usr_pre_init
        jnz 1f

        popl %eax

        pushl (%eax)    // argc
        pushl 4(%eax)   // argv
        
        xorl %eax, %eax
        call main
        
    1:
        movl %eax, %ebx
        movl $__SYSCALL__exit, %eax
        int $LUNAIX_SYS_CALL

        ud2 // should not reach