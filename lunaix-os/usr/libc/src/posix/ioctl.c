#include <lunaix/syscall.h>
#include <lunaix/ioctl.h>
#include <stdarg.h>

int __attribute__((noinline))
ioctl(int fd, int req, ...)
{
    va_list ap;
    va_start(ap, req);

    int ret = do_lunaix_syscall(__SYSCALL_ioctl, fd, req, &ap);
    
    va_end(ap);
    return ret;
}