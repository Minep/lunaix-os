#include "syscall.h"
#include <lunaix/ioctl.h>

__LXSYSCALL2_VARG(int, ioctl, int, fd, int, req);