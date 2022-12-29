#include <lunaix/syscall.h>
#include <usr/sys/ioctl.h>

__LXSYSCALL2_VARG(int, ioctl, int, fd, int, req);