#ifndef __LUNALIBC_SYSCALL_H
#define __LUNALIBC_SYSCALL_H

#include <lunaix/syscallid.h>

unsigned long do_lunaix_syscall(int num, ...);

#endif /* __LUNALIBC_SYSCALL_H */
