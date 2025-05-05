#include <syscall.h>
#include <lunaix/mann.h>
#include <lunaix/types.h>

void*
mmap(void* addr, size_t length, int proct, int flags, int fd, off_t offset)
{
    struct usr_mmap_param mparam = {
        .addr = addr,
        .length = length,
        .proct = proct,
        .flags = flags,
        .fd = fd,
        .offset = offset
    };

    return (void*)do_lunaix_syscall(__NR__lxsys_sys_mmap, &mparam);
}

int
munmap(void* addr, size_t length)
{
    return do_lunaix_syscall(__NR__lxsys_munmap, addr, length);
}
