#ifndef __LUNAIX_TYPES_H
#define __LUNAIX_TYPES_H

#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>

#define PACKED __attribute__((packed))

// TODO: WTERMSIG

// TODO: replace the integer type with these. To make thing more portable.

typedef unsigned char u8_t;
typedef unsigned short u16_t;
typedef unsigned int u32_t;
typedef unsigned long long u64_t;
typedef unsigned long ptr_t;

typedef int64_t lba_t;

#endif /* __LUNAIX_TYPES_H */
