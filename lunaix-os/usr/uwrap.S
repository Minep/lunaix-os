#define __ASM__
#include <lunaix/syscall.h>

.section .data
    .global environ
    environ:
        .long 0

.section .text
    .global _u_start
    _u_start:        
        xorl %eax, %eax
        call main
        
    1:
        movl %eax, %ebx
        movl $__SYSCALL__exit, %eax
        int $LUNAIX_SYS_CALL

        ud2 // should not reach