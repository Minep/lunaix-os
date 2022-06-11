#ifndef __LUNAIX_TYPES_H
#define __LUNAIX_TYPES_H

#include <stdint.h>

#define PROCTERM 0x10000
#define PROCSTOP 0x20000

#define WNOHANG 1
#define WUNTRACED 2
#define WEXITSTATUS(wstatus) ((wstatus & 0xffff))
#define WIFSTOPPED(wstatus) ((wstatus & PROCSTOP))
#define WIFEXITED(wstatus)                                                     \
    ((wstatus & PROCTERM) && ((short)WEXITSTATUS(wstatus) >= 0))

typedef int32_t pid_t;

#endif /* __LUNAIX_TYPES_H */
