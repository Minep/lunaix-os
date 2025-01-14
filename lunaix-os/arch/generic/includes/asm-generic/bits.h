#ifndef __LUNAIX_ARCH_GENERIC_BITS_H
#define __LUNAIX_ARCH_GENERIC_BITS_H

#define _BITS_GENMASK(h, l)     \
        (((1 << ((h) + 1)) - 1) ^ ((1 << (l)) - 1))

#define _BITS_EXTRACT(from, h, l)     \
        (((from) & (((1 << (h + 1)) - 1) ^ ((1 << (l)) - 1))) >> (l))

#define _BITS_INSERT(to, from, h, l)    \
        (((to) & ~_BITS_GENMASK(h, l)) | (((from) << (l)) & _BITS_GENMASK(h, l)))

#define _BITS_STATIC(val, h, l)         \
        (((val) & _BITS_GENMASK(h, l)) << (l))

#endif /* __LUNAIX_ARCH_BITS_H */
