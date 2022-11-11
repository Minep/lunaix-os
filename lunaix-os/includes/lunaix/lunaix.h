#ifndef __LUNAIX_LUNAIX_H
#define __LUNAIX_LUNAIX_H

#include <lunaix/syscall.h>
#include <lunaix/types.h>

__LXSYSCALL(void, yield);

__LXSYSCALL1(pid_t, wait, int*, status);

__LXSYSCALL3(pid_t, waitpid, pid_t, pid, int*, status, int, options);

__LXSYSCALL(int, geterrno);

__LXSYSCALL2_VARG(void, syslog, int, level, const char*, fmt);

#endif /* __LUNAIX_LUNAIX_H */
