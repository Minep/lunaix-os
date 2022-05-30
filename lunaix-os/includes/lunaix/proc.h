#ifndef __LUNAIX_SYS_H
#define __LUNAIX_SYS_H

#include <lunaix/syscall.h>

__LXSYSCALL1(void, exit, int, status)

__LXSYSCALL(void, yield)

#endif /* __LUNAIX_SYS_H */
