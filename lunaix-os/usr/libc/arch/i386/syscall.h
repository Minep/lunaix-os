#ifndef __LUNAIX_SYSCALL_H
#define __LUNAIX_SYSCALL_H
#include <lunaix/syscallid.h>

#define asmlinkage __attribute__((regparm(0)))

#define __PARAM_MAP1(t1, p1) t1 p1
#define __PARAM_MAP2(t1, p1, ...) t1 p1, __PARAM_MAP1(__VA_ARGS__)
#define __PARAM_MAP3(t1, p1, ...) t1 p1, __PARAM_MAP2(__VA_ARGS__)
#define __PARAM_MAP4(t1, p1, ...) t1 p1, __PARAM_MAP3(__VA_ARGS__)
#define __PARAM_MAP5(t1, p1, ...) t1 p1, __PARAM_MAP4(__VA_ARGS__)
#define __PARAM_MAP6(t1, p1, ...) t1 p1, __PARAM_MAP5(__VA_ARGS__)

#define ___DOINT33(callcode, rettype)                                          \
    int v;                                                                     \
    asm volatile("int %1\n" : "=a"(v) : "i"(33), "a"(callcode));               \
    return (rettype)v;

#define __LXSYSCALL(rettype, name)                                             \
    rettype name()                                                             \
    {                                                                          \
        ___DOINT33(__SYSCALL_##name, rettype)                                  \
    }

#define __LXSYSCALL1(rettype, name, t1, p1)                                    \
    rettype name(__PARAM_MAP1(t1, p1))                                         \
    {                                                                          \
        asm("" ::"b"(p1));                                                     \
        ___DOINT33(__SYSCALL_##name, rettype)                                  \
    }

#define __LXSYSCALL2(rettype, name, t1, p1, t2, p2)                            \
    rettype name(__PARAM_MAP2(t1, p1, t2, p2))                                 \
    {                                                                          \
        asm("\n" ::"b"(p1), "c"(p2));                                          \
        ___DOINT33(__SYSCALL_##name, rettype)                                  \
    }

#define __LXSYSCALL3(rettype, name, t1, p1, t2, p2, t3, p3)                    \
    rettype name(__PARAM_MAP3(t1, p1, t2, p2, t3, p3))                         \
    {                                                                          \
        asm("\n" ::"b"(p1), "c"(p2), "d"(p3));                                 \
        ___DOINT33(__SYSCALL_##name, rettype)                                  \
    }

#define __LXSYSCALL4(rettype, name, t1, p1, t2, p2, t3, p3, t4, p4)            \
    rettype name(__PARAM_MAP4(t1, p1, t2, p2, t3, p3, t4, p4))                 \
    {                                                                          \
        asm("\n" ::"b"(p1), "c"(p2), "d"(p3), "D"(p4));                        \
        ___DOINT33(__SYSCALL_##name, rettype)                                  \
    }

#define __LXSYSCALL5(rettype, name, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5)    \
    rettype name(__PARAM_MAP5(t1, p1, t2, p2, t3, p3, t4, p4, t5, p5))         \
    {                                                                          \
        asm("" ::"r"(p5), "b"(p1), "c"(p2), "d"(p3), "D"(p4), "S"(p5));        \
        ___DOINT33(__SYSCALL_##name, rettype)                                  \
    }

#define __LXSYSCALL2_VARG(rettype, name, t1, p1, t2, p2)                       \
    __attribute__((noinline)) rettype name(__PARAM_MAP2(t1, p1, t2, p2), ...)  \
    {                                                                          \
        /* No inlining! This depends on the call frame assumption */           \
        void* _last = (void*)&p2 + sizeof(void*);                              \
        asm("\n" ::"b"(p1), "c"(p2), "d"(_last));                              \
        ___DOINT33(__SYSCALL_##name, rettype)                                  \
    }

#endif /* __LUNAIX_SYSCALL_H */
