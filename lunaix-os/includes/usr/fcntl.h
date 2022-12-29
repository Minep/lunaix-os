#ifndef __LUNAIX_SYS_FCNTL_H
#define __LUNAIX_SYS_FCNTL_H

#include <usr/fcntl_defs.h>
#include <usr/sys/types.h>

int
open(const char* path, int flags);

#endif /* __LUNAIX_FCNTL_H */
