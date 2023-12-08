#ifndef __LUNAIX_MULDIV64_H
#define __LUNAIX_MULDIV64_H

#include <lunaix/spike.h>
#include <lunaix/types.h>

#define do_udiv64(n, base)                                                     \
    ({                                                                         \
        unsigned long __upper, __low, __high, __mod, __base;                   \
        __base = (base);                                                       \
        if (__builtin_constant_p(__base) && is_pot(__base)) {                  \
            __mod = n & (__base - 1);                                          \
            n >>= ILOG2(__base);                                               \
        } else {                                                               \
            asm("" : "=a"(__low), "=d"(__high) : "A"(n));                      \
            __upper = __high;                                                  \
            if (__high) {                                                      \
                __upper = __high % (__base);                                   \
                __high = __high / (__base);                                    \
            }                                                                  \
            asm("divl %2"                                                      \
                : "=a"(__low), "=d"(__mod)                                     \
                : "rm"(__base), "0"(__low), "1"(__upper));                     \
            asm("" : "=A"(n) : "a"(__low), "d"(__high));                       \
        }                                                                      \
        __mod;                                                                 \
    })

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

#endif /* __LUNAIX_MULDIV64_H */
