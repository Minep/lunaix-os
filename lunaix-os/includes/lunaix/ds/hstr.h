#ifndef __LUNAIX_HSTR_H
#define __LUNAIX_HSTR_H

#include <lib/hash.h>

#define HSTR_FULL_HASH 32

struct hstr
{
    unsigned int hash;
    unsigned int len;
    char* value;
};

#define HSTR(str, length)                                                      \
    (struct hstr)                                                              \
    {                                                                          \
        .len = (length), .value = (str)                                        \
    }

#define HSTR_EQ(str1, str2) ((str1)->hash == (str2)->hash)

inline void
hstr_rehash(struct hstr* hash_str, unsigned int truncate_to)
{
    hash_str->hash = strhash_32(hash_str->value, truncate_to);
}

#endif /* __LUNAIX_HSTR_H */
