/**
 * @file fsm.c File system manager
 * @author Lunaixsky (zelong56@gmail.com)
 * @brief
 * @version 0.1
 * @date 2022-07-19
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <klibc/string.h>
#include <lunaix/ds/hashtable.h>
#include <lunaix/fs.h>

#define HASH_BUCKET_BITS 4
#define HASH_BUCKET_NUM (1 << HASH_BUCKET_BITS)

DECLARE_HASHTABLE(fs_registry, HASH_BUCKET_NUM);

void
fsm_init()
{
    hashtable_init(fs_registry);
}

void
fsm_register(struct filesystem* fs)
{
    hstr_rehash(&fs->fs_name, HSTR_FULL_HASH);
    hashtable_hash_in(fs_registry, &fs->fs_list, fs->fs_name.hash);
}

struct filesystem*
fsm_get(const char* fs_name)
{
    struct filesystem *pos, *next;
    struct hstr str = HSTR(fs_name, 0);
    hstr_rehash(&str, HSTR_FULL_HASH);

    hashtable_hash_foreach(fs_registry, str.hash, pos, next, fs_list)
    {
        if (pos->fs_name.hash == str.hash) {
            return pos;
        }
    }

    return NULL;
}