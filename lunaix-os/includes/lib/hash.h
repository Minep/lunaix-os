#ifndef __LUNAIX_HASH_H
#define __LUNAIX_HASH_H

#include <stdint.h>

#define HASH_SIZE_BITS 32

uint32_t
strhash_32(char* str, uint32_t truncate_to);

/**
 * @brief Simple generic hash function
 *
 * ref:
 * https://elixir.bootlin.com/linux/v5.18.12/source/include/linux/hash.h#L60
 *
 * @param val
 * @return uint32_t
 */
inline uint32_t
hash_32(uint32_t val, uint32_t truncate_to)
{
    return (val * 0x61C88647u) >> (HASH_SIZE_BITS - truncate_to);
}

#endif /* __LUNAIX_HASH_H */
