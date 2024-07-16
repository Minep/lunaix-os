#include <lunaix/syscall.h>
#include <lunaix/dirent_defs.h>

int
sys_readdir(int fd, struct lx_dirent* dirent)
{
    return do_lunaix_syscall(__SYSCALL_sys_readdir, fd, dirent);
}