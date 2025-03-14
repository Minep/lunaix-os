#ifndef __LUNAIX_ARCH_BITS_H
#define __LUNAIX_ARCH_BITS_H

#include <asm-generic/bits.h>

#ifndef __ASM__
#undef _BITS_EXTRACT
#undef _BITS_INSERT

#define _BITS_EXTRACT(from, h, l)       \
        ({                              \
            unsigned long _r;           \
            asm ("ubfm %0, %1, %2, %3"  \
                 : "=r"(_r)             \
                 : "r"(from),           \
                   "i"(l),"i"(h));      \
            _r;                         \
        })

#define _BITS_INSERT(to, from, h, l)    \
        ({                              \
            unsigned long _r = to;      \
            asm ("bfi %0, %1, %2, %3"   \
                 : "=r"(_r)             \
                 : "r"(from),           \
                   "i"(l),              \
                   "i"(h - l + 1));     \
            _r;                         \
        })

#endif
#endif /* __LUNAIX_ARCH_BITS_H */
