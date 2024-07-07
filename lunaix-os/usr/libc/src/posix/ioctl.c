#include <lunaix/syscall.h>
#include <lunaix/ioctl.h>

int variadic
ioctl(int fd, int req, ...)
{
    unsigned long va = va_list_addr(req);
    return do_lunaix_syscall(__SYSCALL_ioctl, fd, req, va);
}