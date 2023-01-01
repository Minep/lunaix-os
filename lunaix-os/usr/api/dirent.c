#include <lunaix/syscall.h>
#include <sys/dirent.h>

__LXSYSCALL2(int, sys_readdir, int, fd, struct lx_dirent*, dent)
