#include <klibc/hash.h>
#include <lunaix/compiler.h>

/**
 * @brief Simple string hash function (sdbm)
 *
 * ref: http://www.cse.yorku.ca/~oz/hash.html
 * 
 * sdbm has lower standard deviation in bucket distribution
 * than djb2 (previously used) for low bucket size (16, 32).
 *
 * @param str
 * @return unsigned int
 */
u32_t _weak
strhash_32(const char* str, u32_t truncate_to)
{
    if (!str)
        return 0;

    u32_t hash = 0;
    int c;

    while ((c = *str++))
        hash = (hash << 6) + (hash << 16) + c - hash;

    return hash >> (HASH_SIZE_BITS - truncate_to);
}