#ifndef __LUNAIX_HSTR_H
#define __LUNAIX_HSTR_H

#include <klibc/hash.h>

#define HSTR_FULL_HASH 32

struct hstr
{
    unsigned int hash;
    unsigned int len;
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

#define HSTR_VAL(hstr)  ((hstr).value)
#define HSTR_LEN(hstr)  ((hstr).len)
#define HSTR_HASH(hstr) ((hstr).hash)

inline void
hstr_rehash(struct hstr* hash_str, u32_t truncate_to)
{
    hash_str->hash = strhash_32(hash_str->value, truncate_to);
}

void
hstrcpy(struct hstr* dest, struct hstr* src);

#endif /* __LUNAIX_HSTR_H */
