#define __ASM__
#include <lunaix/syscall.h>

#ifdef CONFIG_ARCH_X86_64
#   define SYSCALL(x)     .8byte x
#   define SIZE    8
#else
#   define SYSCALL(x)     .4byte x
#   define SIZE    4
#endif

.section .data
    .global __syscall_table
    __syscall_table:
        1:
        #include <asm/syscall_nr.inc>
        2:
        .rept __SYSCALL_MAX - (2b - 1b) / SIZE
            .long 0
        .endr