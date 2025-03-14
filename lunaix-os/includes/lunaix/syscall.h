#ifndef __LUNAIX_SYSCALL_H
#define __LUNAIX_SYSCALL_H

#include <usr/lunaix/syscallid.h>

#ifndef __ASM__

#include <lunaix/compiler.h>

#define SYSCALL_ESTATUS(errno) -((errno) != 0)

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

#define __DEFINE_LXSYSCALL5(                                                   \
  rettype, name, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5)                       \
    asmlinkage rettype __lxsys_##name(                                         \
      __PARAM_MAP5(t1, p1, t2, p2, t3, p3, t4, p4, t5, p5))

#endif
#endif /* __LUNAIX_SYSCALL_H */
