#include <syscall.h>
#include <sys/types.h>
#include <stdio.h>

void
yield()
{
    do_lunaix_syscall(__NR__lxsys_yield);
}

pid_t
wait(int* status)
{
    return do_lunaix_syscall(__NR__lxsys_wait, status);
}

pid_t
waitpid(pid_t pid, int* status, int options)
{
    return do_lunaix_syscall(__NR__lxsys_waitpid, pid, status, options);
}

void
syslog(int level, const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);

    unsigned int size = vsnprintf(buf, 1024, fmt, ap);
    do_lunaix_syscall(__NR__lxsys_syslog, level, buf, size);

    va_end(ap);
}

int
realpathat(int fd, char* buf, size_t size)
{
    return do_lunaix_syscall(__NR__lxsys_realpathat, fd, buf, size);
}