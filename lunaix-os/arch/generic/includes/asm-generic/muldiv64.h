#ifndef __LUNAIX_ARCH_GENERIC_MULDIV64_H
#define __LUNAIX_ARCH_GENERIC_MULDIV64_H

#include <lunaix/types.h>

#ifdef CONFIG_ARCH_BITS_64
static inline u64_t
udiv64(u64_t n, unsigned int base)
{
    return n / base;
}

static inline unsigned int
umod64(u64_t n, unsigned int base)
{
    return n % base;
}
#else
#error "no generic muldiv64 for 32 bits arch"
#endif


#endif /* __LUNAIX_ARCH_MULDIV64_H */
