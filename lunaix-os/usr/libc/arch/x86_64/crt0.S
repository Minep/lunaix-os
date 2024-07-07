#define __ASM__
#include <lunaix/syscallid.h>

.section .data
    .global environ
    environ:
        .long 0

.section .text
    .global _start
    _start:      
        xorq %rax, %rax
        xorq %rbp, %rbp
        fninit
        call main
        
    1:
        movq %rax, %rbx
        movq $__SYSCALL__exit, %rax
        int $33

        ud2 // should not reach