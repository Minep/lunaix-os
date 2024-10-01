#ifndef __LUNAIX_BITS_H
#define __LUNAIX_BITS_H

#include <lunaix/compiler.h>

#define BITS(h, l)               (((1UL << ((h) + 1)) - 1) ^ ((1UL << (l)) - 1))
#define BIT(p)                   BITS(p, p)

#define BITS_GET(from, bits)     (((from) & (bits)) >> ctzl(bits))

#define BITS_SET(to, bits, val)  \
            (((to) & ~(bits)) | (((val) << ctzl(bits)) & (bits)))

#endif /* __LUNAIX_BITS_H */
