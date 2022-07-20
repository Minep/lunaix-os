#ifndef __LUNAIX_HSTR_H
#define __LUNAIX_HSTR_H

#include <lib/hash.h>

struct hstr
{
    unsigned int hash;
    unsigned int len;
    char* value;
};

#define HSTR(str, length)                                                      \
    (struct hstr)                                                              \
    {                                                                          \
        .len = length, .value = str                                            \
    }

inline void
hstr_rehash(struct hstr* hash_str, unsigned int truncate_to)
{
    hash_str->hash = strhash_32(hash_str->value, truncate_to);
}

#endif /* __LUNAIX_HSTR_H */
