#ifndef __LUNAIX_TYPES_H
#define __LUNAIX_TYPES_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#define PEXITTERM 0x100
#define PEXITSTOP 0x200
#define PEXITSIG 0x400

#define PEXITNUM(flag, code) (flag | (code & 0xff))

#define WNOHANG 1
#define WUNTRACED 2
#define WEXITSTATUS(wstatus) ((wstatus & 0xff))
#define WIFSTOPPED(wstatus) ((wstatus & PEXITSTOP))
#define WIFEXITED(wstatus)                                                     \
    ((wstatus & PEXITTERM) && ((char)WEXITSTATUS(wstatus) >= 0))

#define WIFSIGNALED(wstatus) ((wstatus & PEXITSIG))

#define PACKED __attribute__((packed))

// TODO: WTERMSIG

// TODO: replace the integer type with these. To make thing more portable.

typedef unsigned char u8_t;
typedef unsigned short u16_t;
typedef unsigned int u32_t;
typedef unsigned long long u64_t;
typedef unsigned long ptr_t;
typedef signed long ssize_t;

typedef int32_t pid_t;
typedef int64_t lba_t;

#endif /* __LUNAIX_TYPES_H */
