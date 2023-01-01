#ifndef __LUNAIX_SYS_FCNTL_H
#define __LUNAIX_SYS_FCNTL_H

#include <fcntl_defs.h>
#include <sys/types.h>

int
open(const char* path, int flags);

#endif /* __LUNAIX_FCNTL_H */
