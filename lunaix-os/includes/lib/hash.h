#ifndef __LUNAIX_HASH_H
#define __LUNAIX_HASH_H

#include <lunaix/types.h>

#define HASH_SIZE_BITS 32

u32_t
strhash_32(const char* str, u32_t truncate_to);

/**
 * @brief Simple generic hash function
 *
 * ref:
 * https://elixir.bootlin.com/linux/v5.18.12/source/include/linux/hash.h#L60
 *
 * @param val
 * @return u32_t
 */
inline u32_t
hash_32(const u32_t val, u32_t truncate_to)
{
    return (val * 0x61C88647u) >> (HASH_SIZE_BITS - truncate_to);
}

#endif /* __LUNAIX_HASH_H */
