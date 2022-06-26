#ifndef __LUNAIX_SYSCALL_H
#define __LUNAIX_SYSCALL_H

#include <arch/x86/vectors.h>

#define __SYSCALL_fork 1
#define __SYSCALL_yield 2
#define __SYSCALL_sbrk 3
#define __SYSCALL_brk 4
#define __SYSCALL_getpid 5
#define __SYSCALL_getppid 6
#define __SYSCALL_sleep 7
#define __SYSCALL__exit 8
#define __SYSCALL_wait 9
#define __SYSCALL_waitpid 10
#define __SYSCALL_sigreturn 11
#define __SYSCALL_sigprocmask 12
#define __SYSCALL_signal 13
#define __SYSCALL_pause 14
#define __SYSCALL_kill 15
#define __SYSCALL_alarm 16
#define __SYSCALL_sigpending 17
#define __SYSCALL_sigsuspend 18

#define __SYSCALL_MAX 0x100

#ifndef __ASM__
void
syscall_install();

#define asmlinkage __attribute__((regparm(0)))

#define __PARAM_MAP1(t1, p1) t1 p1
#define __PARAM_MAP2(t1, p1, ...) t1 p1, __PARAM_MAP1(__VA_ARGS__)
#define __PARAM_MAP3(t1, p1, ...) t1 p1, __PARAM_MAP2(__VA_ARGS__)
#define __PARAM_MAP4(t1, p1, ...) t1 p1, __PARAM_MAP3(__VA_ARGS__)
#define __PARAM_MAP5(t1, p1, ...) t1 p1, __PARAM_MAP4(__VA_ARGS__)
#define __PARAM_MAP6(t1, p1, ...) t1 p1, __PARAM_MAP5(__VA_ARGS__)

#define __DEFINE_LXSYSCALL(rettype, name) asmlinkage rettype __lxsys_##name()

#define __DEFINE_LXSYSCALL1(rettype, name, t1, p1)                             \
    asmlinkage rettype __lxsys_##name(__PARAM_MAP1(t1, p1))

#define __DEFINE_LXSYSCALL2(rettype, name, t1, p1, t2, p2)                     \
    asmlinkage rettype __lxsys_##name(__PARAM_MAP2(t1, p1, t2, p2))

#define __DEFINE_LXSYSCALL3(rettype, name, t1, p1, t2, p2, t3, p3)             \
    asmlinkage rettype __lxsys_##name(__PARAM_MAP3(t1, p1, t2, p2, t3, p3))

#define __DEFINE_LXSYSCALL4(rettype, name, t1, p1, t2, p2, t3, p3, t4, p4)     \
    asmlinkage rettype __lxsys_##name(                                         \
      __PARAM_MAP4(t1, p1, t2, p2, t3, p3, t4, p4))

#define __SYSCALL_INTERRUPTIBLE(code)                                          \
    asm("sti");                                                                \
    { code };                                                                  \
    asm("cli");

#define ___DOINT33(callcode, rettype)                                          \
    int v;                                                                     \
    asm volatile("int %1\n" : "=a"(v) : "i"(LUNAIX_SYS_CALL), "a"(callcode));  \
    return (rettype)v;

#define __LXSYSCALL(rettype, name)                                             \
    static rettype name()                                                      \
    {                                                                          \
        ___DOINT33(__SYSCALL_##name, rettype)                                  \
    }

#define __LXSYSCALL1(rettype, name, t1, p1)                                    \
    static rettype name(__PARAM_MAP1(t1, p1))                                  \
    {                                                                          \
        asm("" ::"b"(p1));                                                     \
        ___DOINT33(__SYSCALL_##name, rettype)                                  \
    }

#define __LXSYSCALL2(rettype, name, t1, p1, t2, p2)                            \
    static rettype name(__PARAM_MAP2(t1, p1, t2, p2))                          \
    {                                                                          \
        asm("\n" ::"b"(p1), "c"(p2));                                          \
        ___DOINT33(__SYSCALL_##name, rettype)                                  \
    }

#define __LXSYSCALL3(rettype, name, t1, p1, t2, p2, t3, p3)                    \
    static rettype name(__PARAM_MAP3(t1, p1, t2, p2, t3, p3))                  \
    {                                                                          \
        asm("\n" ::"b"(p1), "c"(p2), "d"(p3));                                 \
        ___DOINT33(__SYSCALL_##name, rettype)                                  \
    }

#define __LXSYSCALL4(rettype, name, t1, p1, t2, p2, t3, p3, t4, p4)            \
    static rettype name(__PARAM_MAP3(t1, p1, t2, p2, t3, p3, t4, p4))          \
    {                                                                          \
        asm("\n" ::"b"(p1), "c"(p2), "d"(p3), "D"(p4));                        \
        ___DOINT33(__SYSCALL_##name, rettype)                                  \
    }
#endif
#endif /* __LUNAIX_SYSCALL_H */
