#ifndef __LUNALIBC_SYS_LUNAIX_H
#define __LUNALIBC_SYS_LUNAIX_H

#include <sys/types.h>
#include <stddef.h>

void
yield();

void
syslog(int level, const char* fmt, ...);

int
realpathat(int fd, char* buf, size_t size);

#endif /* __LUNALIBC_LUNAIX_H */
