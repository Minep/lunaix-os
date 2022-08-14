/**
 * @file hashtable.h Simple hash table implementation.
 * Based on
 * https://elixir.bootlin.com/linux/v5.18.12/source/include/linux/hashtable.h
 * @author Lunaixsky (zelong56@gmail.com)
 * @brief
 * @version 0.1
 * @date 2022-07-20
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef __LUNAIX_HASHTABLE_H
#define __LUNAIX_HASHTABLE_H

#include <lib/hash.h>
#include <lunaix/ds/llist.h>

struct hbucket
{
    struct hlist_node* head;
};

#define __hashkey(table, hash) (hash % (sizeof(table) / sizeof(table[0])))

#define DECLARE_HASHTABLE(name, bucket_num) struct hbucket name[bucket_num];

#define hashtable_bucket_foreach(bucket, pos, n, member)                       \
    for (pos = list_entry((bucket)->head, typeof(*pos), member);               \
         pos && ({                                                             \
             n = list_entry(pos->member.next, typeof(*pos), member);           \
             1;                                                                \
         });                                                                   \
         pos = n)

#define hashtable_hash_foreach(table, hash, pos, n, member)                    \
    hashtable_bucket_foreach(&table[__hashkey(table, hash)], pos, n, member)

#define hashtable_init(table)                                                  \
    {                                                                          \
        for (int i = 0; i < (sizeof(table) / sizeof(table[0])); i++) {         \
            table[i].head = 0;                                                 \
        }                                                                      \
    }

#define hashtable_hash_in(table, list_node, hash)                              \
    hlist_add(&table[__hashkey(table, hash)].head, list_node)

#endif /* __LUNAIX_HASHTABLE_H */
