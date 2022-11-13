#include <lib/hash.h>

/**
 * @brief Simple string hash function
 *
 * ref: https://stackoverflow.com/a/7666577
 *
 * @param str
 * @return unsigned int
 */
u32_t
strhash_32(const char* str, u32_t truncate_to)
{
    if (!str)
        return 0;

    u32_t hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash >> (HASH_SIZE_BITS - truncate_to);
}