#define __ASM__
#include <lunaix/syscallid.h>

.section .data
    .global environ
    environ:
        .long 0

.section .text
    .global _start
    _start:      
        xorl %eax, %eax
        xorl %ebp, %ebp
        fninit
        call main
        
    1:
        movl %eax, %ebx
        movl $__SYSCALL__exit, %eax
        int $33

        ud2 // should not reach