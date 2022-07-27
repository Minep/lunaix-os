#include <lib/hash.h>

/**
 * @brief Simple string hash function
 *
 * ref: https://stackoverflow.com/a/7666577
 *
 * @param str
 * @return unsigned int
 */
uint32_t
strhash_32(unsigned char* str, uint32_t truncate_to)
{
    if (!str)
        return 0;

    uint32_t hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash >> (HASH_SIZE_BITS - truncate_to);
}