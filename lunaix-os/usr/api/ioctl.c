#include <lunaix/syscall.h>
#include <sys/ioctl.h>

__LXSYSCALL2_VARG(int, ioctl, int, fd, int, req);