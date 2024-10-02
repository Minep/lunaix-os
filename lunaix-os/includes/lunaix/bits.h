#ifndef __LUNAIX_BITS_H
#define __LUNAIX_BITS_H

#include <lunaix/compiler.h>
#include <asm/bits.h>

#define BITFIELD(h, l)              (h), (l)

#define BIT(p)                   BITFIELD(p, p)

#define BITS_GENMASK(bits)       _BITS_GENMASK(bits)

#define BITS_GET(from, bits)     _BITS_EXTRACT(from, bits)

#define BITS_SET(to, bits, val)  _BITS_INSERT(to, val, bits)

#endif /* __LUNAIX_BITS_H */
