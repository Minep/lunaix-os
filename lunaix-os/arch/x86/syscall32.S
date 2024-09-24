#define __ASM__
#include <lunaix/syscall.h>
#include "syscall_nr.inc"

.section .text
    .type syscall_hndlr, @function
    .global syscall_hndlr
    syscall_hndlr:
        pushl   %ebp
        movl    %esp, %ebp
        movl    8(%esp), %ebx          // struct hart_state*

        addl    $4, %ebx
        movl    (%ebx), %eax          /* eax: call code as well as the return value from syscall */
        cmpl    $__SYSCALL_MAX, %eax
        jae     2f

        shll    $2, %eax
        addl    $syscall_table, %eax
        cmpl    $0, (%eax)
        jne     1f
    2:    
        neg     %eax
        movl    %ebp, %esp
        popl    %ebp
        ret

    1:
        pushl   %ebx
        pushl   24(%ebx)      /* esi - #5 arg */
        pushl   16(%ebx)      /* edi - #4 arg */
        pushl   12(%ebx)      /* edx - #3 arg */
        pushl   8(%ebx)       /* ecx - #2 arg */
        pushl   4(%ebx)       /* ebx - #1 arg */
        pushl    (%eax)
        
        call    dispatch_syscall

        addl    $24, %esp      /* remove the parameters from stack */
        
        popl    %ebx
        movl    %eax, (%ebx)    /* save the return value */
        
        movl    %ebp, %esp
        popl    %ebp
        
        ret