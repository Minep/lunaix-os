#ifndef __LUNALIBC_SYS_FCNTL_H
#define __LUNALIBC_SYS_FCNTL_H

#include <lunaix/fcntl.h>
#include <sys/types.h>

extern int
open(const char* path, int flags);

extern int
fstat(int fd, struct file_stat* stat);

#endif /* __LUNALIBC_FCNTL_H */
