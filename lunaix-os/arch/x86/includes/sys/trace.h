#ifndef __LUNAIX_ARCH_TRACE_H
#define __LUNAIX_ARCH_TRACE_H

#include <lunaix/types.h>

static inline bool 
arch_valid_fp(ptr_t ptr) {
    extern int __bsskstack_end[];
    extern int __bsskstack_start[];
    return ((ptr_t)__bsskstack_start <= ptr && ptr <= (ptr_t)__bsskstack_end);
}


#endif /* __LUNAIX_TRACE_H */
