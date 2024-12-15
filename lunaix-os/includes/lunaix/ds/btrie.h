#ifndef __LUNAIX_SPARSE_H
#define __LUNAIX_SPARSE_H

#include <lunaix/ds/llist.h>
#include <lunaix/types.h>

#define BTRIE_BITS 4

/**
 * key-sorted prefix tree
 */
struct btrie
{
    struct btrie_node* btrie_root;
    unsigned int order;
};

struct btrie_node
{
    struct llist_header nodes;
    struct btrie_node* parent;
    unsigned long index;
    void* data;

    struct btrie_node** children;
    int children_cnt;
};

void
btrie_init(struct btrie* btrie, unsigned int order);

void*
btrie_get(struct btrie* root, unsigned long index);

/**
 * Set an object into btrie tree at given location,
 * override if another object is already present.
 */
void
btrie_set(struct btrie* root, unsigned long index, void* data);

/**
 * Map an object into btrie tree, return the index the object
 * mapped to
 */
unsigned long
btrie_map(struct btrie* root, 
          unsigned long start, unsigned long end, void* data);

void*
btrie_remove(struct btrie* root, unsigned long index);

void
btrie_release(struct btrie* tree);

#endif /* __LUNAIX_SPARSE_H */
