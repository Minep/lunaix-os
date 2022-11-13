#ifndef __LUNAIX_SPARSE_H
#define __LUNAIX_SPARSE_H

#include <lunaix/ds/llist.h>
#include <lunaix/types.h>

#define BTRIE_BITS 4

struct btrie
{
    struct btrie_node* btrie_root;
    int truncated;
};

struct btrie_node
{
    struct llist_header children;
    struct llist_header siblings;
    struct llist_header nodes;
    struct btrie_node* parent;
    u32_t index;
    void* data;
};

void
btrie_init(struct btrie* btrie, u32_t trunc_bits);

void*
btrie_get(struct btrie* root, u32_t index);

void
btrie_set(struct btrie* root, u32_t index, void* data);

void*
btrie_remove(struct btrie* root, u32_t index);

void
btrie_release(struct btrie* tree);

#endif /* __LUNAIX_SPARSE_H */
