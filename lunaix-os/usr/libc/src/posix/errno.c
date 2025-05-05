#include <syscall.h>
#include <errno.h>

int
geterrno()
{
    return do_lunaix_syscall(__NR__lxsys_geterrno);
}