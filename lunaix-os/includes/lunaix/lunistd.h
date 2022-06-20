#ifndef __LUNAIX_UNISTD_H
#define __LUNAIX_UNISTD_H

#include <lunaix/syscall.h>
#include <lunaix/types.h>

__LXSYSCALL(pid_t, fork)

__LXSYSCALL1(int, sbrk, void*, addr)

__LXSYSCALL1(void*, brk, unsigned long, size)

__LXSYSCALL(pid_t, getpid)

__LXSYSCALL(pid_t, getppid)

__LXSYSCALL1(void, _exit, int, status)

__LXSYSCALL1(unsigned int, sleep, unsigned int, seconds)

__LXSYSCALL(int, pause)

#endif /* __LUNAIX_UNISTD_H */
