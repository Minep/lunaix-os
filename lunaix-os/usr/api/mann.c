#include <lunaix/syscall.h>
#include <sys/mann.h>

__LXSYSCALL2_VARG(void*, sys_mmap, void*, addr, size_t, length);

void*
mmap(void* addr, size_t length, int proct, int flags, int fd, off_t offset)
{
    return sys_mmap(addr, length, proct, fd, offset, flags);
}

__LXSYSCALL2(void, munmap, void*, addr, size_t, length)
