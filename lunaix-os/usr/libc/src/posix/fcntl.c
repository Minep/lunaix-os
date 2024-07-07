#include <lunaix/syscall.h>
#include <fcntl.h>

int
open(const char* path, int options)
{
    return do_lunaix_syscall(__SYSCALL_open, path, options);
}

int
fstat(int fd, struct file_stat* stat)
{
    return do_lunaix_syscall(__SYSCALL_fstat, fd, stat);
}