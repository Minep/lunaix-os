#include "syscall.h"
#include <lunaix/types.h>

__LXSYSCALL(void, yield);

__LXSYSCALL1(pid_t, wait, int*, status);

__LXSYSCALL3(pid_t, waitpid, pid_t, pid, int*, status, int, options);

__LXSYSCALL2_VARG(void, syslog, int, level, const char*, fmt);

__LXSYSCALL3(int, realpathat, int, fd, char*, buf, size_t, size)
