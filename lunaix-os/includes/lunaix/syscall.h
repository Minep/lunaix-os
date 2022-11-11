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
#define __SYSCALL_open 19
#define __SYSCALL_close 20

#define __SYSCALL_read 21
#define __SYSCALL_write 22
#define __SYSCALL_readdir 23
#define __SYSCALL_mkdir 24
#define __SYSCALL_lseek 25
#define __SYSCALL_geterrno 26
#define __SYSCALL_readlink 27
#define __SYSCALL_readlinkat 28
#define __SYSCALL_rmdir 29

#define __SYSCALL_unlink 30
#define __SYSCALL_unlinkat 31
#define __SYSCALL_link 32
#define __SYSCALL_fsync 33
#define __SYSCALL_dup 34
#define __SYSCALL_dup2 35
#define __SYSCALL_realpathat 36
#define __SYSCALL_symlink 37
#define __SYSCALL_chdir 38
#define __SYSCALL_fchdir 39
#define __SYSCALL_getcwd 40
#define __SYSCALL_rename 41
#define __SYSCALL_mount 42
#define __SYSCALL_unmount 43
#define __SYSCALL_getxattr 44
#define __SYSCALL_setxattr 45
#define __SYSCALL_fgetxattr 46
#define __SYSCALL_fsetxattr 47

#define __SYSCALL_ioctl 48
#define __SYSCALL_getpgid 49
#define __SYSCALL_setpgid 50

#define __SYSCALL_syslog 51

#define __SYSCALL_MAX 0x100

#ifndef __ASM__

#define SYSCALL_ESTATUS(errno) -((errno) != 0)

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
    static rettype name(__PARAM_MAP4(t1, p1, t2, p2, t3, p3, t4, p4))          \
    {                                                                          \
        asm("\n" ::"b"(p1), "c"(p2), "d"(p3), "D"(p4));                        \
        ___DOINT33(__SYSCALL_##name, rettype)                                  \
    }

#define __LXSYSCALL2_VARG(rettype, name, t1, p1, t2, p2)                       \
    __attribute__((noinline)) static rettype name(                             \
      __PARAM_MAP2(t1, p1, t2, p2), ...)                                       \
    {                                                                          \
        /* No inlining! This depends on the call frame assumption */           \
        void* _last = (void*)&p2 + sizeof(void*);                              \
        asm("\n" ::"b"(p1), "c"(p2), "d"(_last));                              \
        ___DOINT33(__SYSCALL_##name, rettype)                                  \
    }
#endif

#endif /* __LUNAIX_SYSCALL_H */
