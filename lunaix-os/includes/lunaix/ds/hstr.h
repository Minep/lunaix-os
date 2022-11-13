#ifndef __LUNAIX_HSTR_H
#define __LUNAIX_HSTR_H

#include <lib/hash.h>

#define HSTR_FULL_HASH 32

struct hstr
{
    u32_t hash;
    u32_t len;
    const char* value;
};

#define HSTR(str, length)                                                      \
    (struct hstr)                                                              \
    {                                                                          \
        .len = (length), .value = (str)                                        \
    }

#define HHSTR(str, length, strhash)                                            \
    (struct hstr)                                                              \
    {                                                                          \
        .len = (length), .value = (str), .hash = (strhash)                     \
    }

#define HSTR_EQ(str1, str2) ((str1)->hash == (str2)->hash)

inline void
hstr_rehash(struct hstr* hash_str, u32_t truncate_to)
{
    hash_str->hash = strhash_32(hash_str->value, truncate_to);
}

void
hstrcpy(struct hstr* dest, struct hstr* src);

#endif /* __LUNAIX_HSTR_H */
