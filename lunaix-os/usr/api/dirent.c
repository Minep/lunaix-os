#include <lunaix/syscall.h>
#include <usr/sys/dirent.h>

__LXSYSCALL2(int, sys_readdir, int, fd, struct lx_dirent*, dent)
