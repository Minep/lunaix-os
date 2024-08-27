#ifndef __LUNAIX_TYPES_H
#define __LUNAIX_TYPES_H

#include <lunaix/compiler.h>
#include <stdarg.h>
#include <usr/lunaix/types.h>

// TODO: WTERMSIG

// TODO: replace the integer type with these. To make thing more portable.

typedef unsigned char u8_t;
typedef unsigned short u16_t;
typedef unsigned int u32_t;
typedef unsigned long ptr_t;
typedef unsigned long reg_t;

#ifndef CONFIG_ARCH_BITS_64
typedef unsigned long long u64_t;
#else
typedef unsigned long u64_t;
#endif

typedef int pid_t;
typedef signed long ssize_t;

typedef unsigned int cpu_t;

typedef u64_t lba_t;

#define true 1
#define false 0
typedef int bool;

/**
 * container_of - cast a member of a structure out to the containing structure
 *
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member)                                        \
    ({                                                                         \
        const typeof(((type*)0)->member)* __mptr = (ptr);                      \
        ((ptr_t)ptr != 0UL) ? (type*)((char*)__mptr - offsetof(type, member)) : 0;           \
    })

#define offset(data, off)   \
            ((typeof(data))(__ptr(data) + (off)))

#define offset_t(data, type, off)   \
            ((type*)(__ptr(data) + (off)))

#define __ptr(val)      ((ptr_t)(val))

typedef va_list* sc_va_list;

#endif /* __LUNAIX_TYPES_H */
