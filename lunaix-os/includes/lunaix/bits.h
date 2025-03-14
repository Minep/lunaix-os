#ifndef __LUNAIX_BITS_H
#define __LUNAIX_BITS_H

#include <asm/bits.h>

#define BITS_AT(val, bitfield)       _BITS_STATIC(val, bitfield)

#ifndef __ASM__
#include <lunaix/compiler.h>

#define BITFIELD(h, l)               (h), (l)

#define BIT(p)                       BITFIELD(p, p)
#define BITFLAG(p)                   (1UL << (p))

#define BITS_GENMASK(bitfield)       _BITS_GENMASK(bitfield)

#define BITS_GET(from, bitfield)     _BITS_EXTRACT(from, bitfield)

#define BITS_SET(to, bitfield, val)  _BITS_INSERT(to, val, bitfield)

#endif

#endif /* __LUNAIX_BITS_H */
