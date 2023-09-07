#ifndef __LUNAIX_SYS_FCNTL_H
#define __LUNAIX_SYS_FCNTL_H

#include <lunaix/fcntl_defs.h>
#include <lunaix/types.h>

extern int
open(const char* path, int flags);

extern int
fstat(int fd, struct file_stat* stat);

#endif /* __LUNAIX_FCNTL_H */
