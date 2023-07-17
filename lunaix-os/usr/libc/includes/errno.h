#ifndef __LUNAIX_SYS_ERRNO_H
#define __LUNAIX_SYS_ERRNO_H

#include <lunaix/status.h>

extern int geterrno();

#define errno (geterrno())

#endif /* __LUNAIX_ERRNO_H */
