#ifndef __LUNALIBC_SYS_ERRNO_H
#define __LUNALIBC_SYS_ERRNO_H

#include <lunaix/status.h>

extern int geterrno();

#define errno (geterrno())

#endif /* __LUNALIBC_ERRNO_H */
