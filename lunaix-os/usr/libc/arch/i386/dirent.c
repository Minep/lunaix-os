#include "syscall.h"
#include <lunaix/dirent_defs.h>

__LXSYSCALL2(int, sys_readdir, int, fd, struct lx_dirent*, dent)
