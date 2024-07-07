#include <lunaix/syscall.h>
#include <lunaix/types.h>
#include <stdio.h>

void
yield()
{
    do_lunaix_syscall(__SYSCALL_yield);
}

pid_t
wait(int* status)
{
    return do_lunaix_syscall(__SYSCALL_wait, status);
}

pid_t
waitpid(pid_t pid, int* status, int options)
{
    return do_lunaix_syscall(__SYSCALL_waitpid, pid, status, options);
}

void
syslog(int level, const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);

    vsnprintf(buf, 1024, fmt, ap);
    do_lunaix_syscall(__SYSCALL_syslog, level, buf);

    va_end(ap);
}

int
realpathat(int fd, char* buf, size_t size)
{
    return do_lunaix_syscall(__SYSCALL_realpathat, fd, buf, size);
}