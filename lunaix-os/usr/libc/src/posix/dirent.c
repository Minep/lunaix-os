#include <syscall.h>
#include <lunaix/dirent.h>

int
sys_readdir(int fd, struct lx_dirent* dirent)
{
    return do_lunaix_syscall(__NR__lxsys_sys_readdir, fd, dirent);
}