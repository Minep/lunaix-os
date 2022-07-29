#ifndef __LUNAIX_SYS_H
#define __LUNAIX_SYS_H

#include <lunaix/syscall.h>
#include <lunaix/types.h>

__LXSYSCALL(void, yield);

__LXSYSCALL1(pid_t, wait, int*, status);

__LXSYSCALL3(pid_t, waitpid, pid_t, pid, int*, status, int, options);

__LXSYSCALL(int, geterrno);

#endif /* __LUNAIX_SYS_H */
