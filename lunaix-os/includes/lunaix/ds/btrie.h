#ifndef __LUNAIX_SPARSE_H
#define __LUNAIX_SPARSE_H

#include <lunaix/ds/llist.h>
#include <lunaix/types.h>

#define BTRIE_BITS 4

struct btrie
{
    struct btrie_node* btrie_root;
    unsigned int order;
};

struct btrie_node
{
    struct llist_header children;
    struct llist_header siblings;
    struct llist_header nodes;
    struct btrie_node* parent;
    unsigned long index;
    void* data;
};

void
btrie_init(struct btrie* btrie, unsigned int order);

void*
btrie_get(struct btrie* root, unsigned long index);

void
btrie_set(struct btrie* root, unsigned long index, void* data);

void*
btrie_remove(struct btrie* root, unsigned long index);

void
btrie_release(struct btrie* tree);

#endif /* __LUNAIX_SPARSE_H */
