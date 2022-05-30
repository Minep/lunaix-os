#ifndef __LUNAIX_SYSCALL_H
#define __LUNAIX_SYSCALL_H

#include <arch/x86/vectors.h>

#define __SYSCALL_fork    0x1
#define __SYSCALL_yield   0x2
#define __SYSCALL_exit    0x3
#define __SYSCALL_sbrk    0x4
#define __SYSCALL_brk     0x5

#define __SYSCALL_MAX     0x100

#ifndef __ASM__
void syscall_install();

static void* syscall(unsigned int callcode) {
    asm volatile(
        "int %0"
        ::"i"(LUNAIX_SYS_CALL), "D"(callcode)
        :"eax"
    );
}

#define ___DOINT33(callcode, rettype)\
    int v;    \
    asm volatile (  \
        "int %1\n"    \
        :"=a"(v)    \
        :"i"(LUNAIX_SYS_CALL), "a"(callcode));  \
    return (rettype)v;     \

#define __LXSYSCALL(rettype, name) \
    rettype name () { \
        ___DOINT33(__SYSCALL_##name, rettype) \
    }

#define __LXSYSCALL1(rettype, name, t1, p1) \
    rettype name (t1 p1) { \
        asm (""::"b"(p1)); \
        ___DOINT33(__SYSCALL_##name, rettype) \
    }

#define __LXSYSCALL2(rettype, name, t1, p1, t2, p2) \
    rettype name (t1 p1, t2 p2) { \
        asm ("\n"::"b"(p1), "c"(p2)); \
        ___DOINT33(__SYSCALL_##name, rettype) \
    }

#define __LXSYSCALL3(rettype, name, t1, p1, t2, p2, t3, p3) \
    rettype name (t1 p1, t2 p2, t3 p3) { \
        asm ("\n"::"b"(p1), "c"(p2), "d"(p3)); \
        ___DOINT33(__SYSCALL_##name, rettype) \
    }

#define __LXSYSCALL4(rettype, name, t1, p1, t2, p2, t3, p3, t4, p4) \
    rettype name (t1 p1, t2 p2, t3 p3, t4 p4) { \
        asm ("\n"::"b"(p1), "c"(p2), "d"(p3), "D"(p4)); \
        ___DOINT33(__SYSCALL_##name, rettype) \
    }
#endif
#endif /* __LUNAIX_SYSCALL_H */
