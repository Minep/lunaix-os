#ifndef __LUNALIBC_SYS_LUNAIX_H
#define __LUNALIBC_SYS_LUNAIX_H

#include <lunaix/types.h>
#include <stddef.h>

void
yield();

pid_t
wait(int* status);

pid_t
waitpid(pid_t pid, int* status, int flags);

void
syslog(int level, const char* fmt, ...);

int
realpathat(int fd, char* buf, size_t size);

#endif /* __LUNALIBC_LUNAIX_H */
