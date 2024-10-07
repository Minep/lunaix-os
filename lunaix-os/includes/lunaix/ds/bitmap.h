#ifndef __LUNAIX_BITMAP_H
#define __LUNAIX_BITMAP_H

#include <lunaix/types.h>
#include <lunaix/spike.h>
#include <klibc/string.h>

// first bit of a bitmap chunk placed at the most significant bit
#define BMP_ORIENT_MSB      0
// first bit of a bitmap chunk placed at the least significant bit
#define BMP_ORIENT_LSB      1

#define BMP_NAME(name)          bitmap_##name
#define BMP_PARAM_NAME          bmp
#define BMP_PARAM(name)         struct BMP_NAME(name) *BMP_PARAM_NAME
#define BMP_RAWBYTES(bits)      (((bits) + 7) / 8)
#define BMP_SIZE(t, bits)       \
            (BMP_RAWBYTES(bits) + (BMP_RAWBYTES(bits) % sizeof(t)))

#define BMP_LEN(t, bits)        (BMP_SIZE(t, bits) / sizeof(t))

#define _BITMAP_STRUCT(type, size, name, orient)  struct BMP_NAME(name)


#define _DECLARE_BMP(type, size, name, orient)                                  \
struct BMP_NAME(name) {                                                         \
    type *_map;                                                                 \
    unsigned long nr_bits;                                                      \
}

#define _DECLARE_BMP_STATIC(type, nr_bits, name, orient)                        \
struct BMP_NAME(name) {                                                         \
    type _map[BMP_LEN(type, nr_bits)];                                          \
}

#define BMP_STRCUT_MAP          BMP_PARAM_NAME->_map
#define BMP_STRCUT_NRB          BMP_PARAM_NAME->nr_bits

#define _BMP_OP_CALL(type, size, name, orient, suffix, ...)                     \
bitmap_##name##_##suffix##(__VA_ARGS__)

#define _DEFINE_BMP_INIT_OP(type, size, name, orient, allocfn)                  \
static inline void                                                              \
bitmap_##name##_init_with(BMP_PARAM(name), unsigned long nr_bits, ptr_t map)    \
{                                                                               \
(void)(is_const(size) ? ({                                                      \
    BMP_STRCUT_MAP = map;                                                       \
    BMP_STRCUT_NRB = nr_bits;                                                   \
    memset(BMP_STRCUT_MAP, 0, BMP_SIZE(type, nr_bits) * sizeof(type));0;        \
}) : ({                                                                         \
    memset(BMP_STRCUT_MAP, 0, sizeof(BMP_STRCUT_MAP));0;                        \
}));                                                                            \
}                                                                               \
static inline void                                                              \
bitmap_##name##_init(BMP_PARAM(name), unsigned long nr_bits)                    \
{                                                                               \
(void)(is_const(size) ? ({                                                      \
    bitmap_##name##_init_with(BMP_PARAM_NAME, nr_bits,                          \
                            allocfn(BMP_SIZE(type, nr_bits)));0;                \
}) : ({                                                                         \
    bitmap_##name##_init_with(BMP_PARAM_NAME, nr_bits, NULL);0;                 \
}));                                                                            \
}


#define _DEFINE_BMP_QUERY_OP(type, size, name, orient)                          \
static inline bool                                                              \
bitmap_##name##_query(BMP_PARAM(name), unsigned long pos)                       \
{                                                                               \
    assert(pos < size);                                                         \
    unsigned long n = pos / (sizeof(type) * 8);                                 \
    int i = pos % (sizeof(type) * 8);                                           \
    type at = BMP_STRCUT_MAP[n];                                                     \
    type msk = 1 << select(orient == BMP_ORIENT_MSB,                            \
                            sizeof(type) * 8 - 1 - i, i );                      \
    return !!(at & msk);                                                        \
}


#define _DEFINE_BMP_SET_OP(type, size, name, orient)                            \
static inline void                                                              \
bitmap_##name##_set(BMP_PARAM(name), unsigned long pos, bool val)               \
{                                                                               \
    assert(pos < size);                                                         \
    unsigned long n = pos / (sizeof(type) * 8);                                 \
    int i = pos % (sizeof(type) * 8);                                           \
    type at = BMP_STRCUT_MAP[n];                                          \
    unsigned int off = select(orient == BMP_ORIENT_MSB,                         \
                            sizeof(type) * 8 - 1 - i, i );                      \
    BMP_STRCUT_MAP[n] = (at & ~(1 << off)) | (!!val << off);              \
}


#define _DEFINE_BMP_ALLOCFROM_OP(type, size, name, orient)                      \
static inline bool                                                              \
bitmap_##name##_alloc_from(BMP_PARAM(name), unsigned long start,                \
                            unsigned long* _out)                                \
{                                                                               \
    unsigned long i, p = 0;                                                     \
    int shift;                                                                  \
    type u;                                                                     \
    i = start / 8 / sizeof(type);                                               \
    shift = select(orient == BMP_ORIENT_MSB, sizeof(type) * 8 - 1, 0);          \
    while ((u = BMP_STRCUT_MAP[i]) == (type)-1) i++;                            \
    while ((u & (type)(1U << shift)) && p++ < sizeof(type) * 8)                 \
        select(orient == BMP_ORIENT_MSB, u <<= 1, u >>= 1);                     \
    if (p < sizeof(type) * 8)                                                   \
        return false;                                                           \
    BMP_STRCUT_MAP[i] |= 1UL << shift;                                          \
    *_out = (i + p);                                                            \
    return true;                                                                \
}


#define PREP_STATIC_BITMAP(type, name, nr_bits, orient)                         \
            type, (nr_bits), name, orient
#define PREP_BITMAP(type, name, orient)                                         \
            type, (BMP_STRCUT_NRB), name, orient


#define DECLARE_BITMAP(bmpdef)              _DECLARE_BMP(bmpdef)
#define DECLARE_STATIC_BITMAP(bmpdef)       _DECLARE_BMP_STATIC(bmpdef)
#define BITMAP(bmpdef)                      _BITMAP_STRUCT(bmpdef)

#define DEFINE_BMP_INIT_OP(bmpdef, allocfn) _DEFINE_BMP_INIT_OP(bmpdef, allocfn)

#define DEFINE_BMP_QUERY_OP(bmpdef)         _DEFINE_BMP_QUERY_OP(bmpdef)
#define DEFINE_BMP_SET_OP(bmpdef)           _DEFINE_BMP_SET_OP(bmpdef)
#define DEFINE_BMP_ALLOCFROM_OP(bmpdef)     _DEFINE_BMP_ALLOCFROM_OP(bmpdef)

#define bitmap_query(bitmap, bmp, pos)      \
    _BMP_OP_CALL(bitmap, query, bmp, pos) 

#define bitmap_set(bitmap, bmp, pos, val)      \
    _BMP_OP_CALL(bitmap, set, bmp, pos, val) 

#define bitmap_alloc(bitmap, bmp, start, out)      \
    _BMP_OP_CALL(bitmap, alloc_from, bmp, start, out) 

#define bitmap_init(bitmap, bmp, nr_bits)      \
    _BMP_OP_CALL(bitmap, init, bmp, nr_bits) 

#define bitmap_init_ptr(bitmap, bmp, nr_bits, ptr)      \
    _BMP_OP_CALL(bitmap, init_with, bmp, nr_bits, ptr) 


#endif /* __LUNAIX_BITMAP_H */
