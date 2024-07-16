#include <lunaix/syscall.h>

int
mount(const char* source, const char* target,
      const char* fstype, int options)
{
    return do_lunaix_syscall(__SYSCALL_mount, source, target, fstype, options);
}

int
unmount(const char* target)
{
    return do_lunaix_syscall(__SYSCALL_unmount, target);
}