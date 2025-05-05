#define __ASM__
#include <lunaix/syscallid.h>

.section .data
    .global environ
    environ:
        .long 0

.section .text
    .global _start
    _start:      
        xorl %ebp, %ebp
        movl %esp, %eax
        andl $-16, %esp
        
        leal 4(%eax), %ebx
        pushl %ebx
        pushl (%eax)

        fninit

        xorl %eax, %eax
        call main
        
    1:
        movl %eax, %ebx
        movl $__NR__lxsys_exit, %eax
        int $33

        ud2 // should not reach