#include <lunaix/syscall.h>
#include <errno.h>

int
geterrno()
{
    return do_lunaix_syscall(__SYSCALL_geterrno);
}