#include <lunaix/syscall.h>
#include <lunaix/types.h>

void*
mmap(void* addr, size_t length, int proct, int flags, int fd, off_t offset)
{
    unsigned long va = va_list_addr(length);
    return (void*)do_lunaix_syscall(__SYSCALL_sys_mmap, 
                             addr, length, va);
}

pid_t
munmap(void* addr, size_t length)
{
    return do_lunaix_syscall(__SYSCALL_munmap, addr, length);
}
