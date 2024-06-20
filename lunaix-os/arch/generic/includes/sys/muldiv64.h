#ifndef __LUNAIX_ARCH_MULDIV64_H
#define __LUNAIX_ARCH_MULDIV64_H


#include <lunaix/spike.h>
#include <lunaix/types.h>

#define do_udiv64(n, base)                                                     \
    ({ 0; })

static inline u64_t
udiv64(u64_t n, unsigned int base)
{
    do_udiv64(n, base);

    return n;
}

static inline unsigned int
umod64(u64_t n, unsigned int base)
{
    return do_udiv64(n, base);
}


#endif /* __LUNAIX_ARCH_MULDIV64_H */
