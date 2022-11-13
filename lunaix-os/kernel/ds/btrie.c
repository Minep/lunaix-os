/**
 * @file btrie.c
 * @author Lunaixsky
 * @brief Bitwise trie tree implementation for sparse array.
 * @version 0.1
 * @date 2022-08-01
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <lunaix/ds/btrie.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

#define BTRIE_INSERT 1

struct btrie_node*
__btrie_traversal(struct btrie* root, u32_t index, int options)
{
    index = index >> root->truncated;
    u32_t lz = index ? ROUNDDOWN(31 - __builtin_clz(index), BTRIE_BITS) : 0;
    u32_t bitmask = ((1 << BTRIE_BITS) - 1) << lz;
    u32_t i = 0;
    struct btrie_node* tree = root->btrie_root;

    // Time complexity: O(log_2(log_2(N))) where N is the index to lookup
    while (bitmask && tree) {
        i = (index & bitmask) >> lz;
        struct btrie_node *subtree = 0, *pos, *n;

        llist_for_each(pos, n, &tree->children, siblings)
        {
            if (pos->index == i) {
                subtree = pos;
                break;
            }
        }

        if (!subtree && (options & BTRIE_INSERT)) {
            struct btrie_node* new_tree = vzalloc(sizeof(struct btrie_node));
            new_tree->index = i;
            new_tree->parent = tree;
            llist_init_head(&new_tree->children);

            llist_append(&tree->children, &new_tree->siblings);
            llist_append(&root->btrie_root->nodes, &new_tree->nodes);
            tree = new_tree;
        } else {
            tree = subtree;
        }
        bitmask = bitmask >> BTRIE_BITS;
        lz -= BTRIE_BITS;
    }

    return tree;
}

void
btrie_init(struct btrie* btrie, u32_t trunc_bits)
{
    btrie->btrie_root = vzalloc(sizeof(struct btrie_node));
    llist_init_head(&btrie->btrie_root->nodes);
    llist_init_head(&btrie->btrie_root->children);
    btrie->truncated = trunc_bits;
}

void*
btrie_get(struct btrie* root, u32_t index)
{
    struct btrie_node* node = __btrie_traversal(root, index, 0);
    if (!node) {
        return NULL;
    }
    return node->data;
}

void
btrie_set(struct btrie* root, u32_t index, void* data)
{
    struct btrie_node* node = __btrie_traversal(root, index, BTRIE_INSERT);
    node->data = data;
}

void
__btrie_rm_recursive(struct btrie_node* node)
{
    struct btrie_node* parent = node->parent;

    if (parent) {
        llist_delete(&node->siblings);
        llist_delete(&node->nodes);
        vfree(node);
        if (llist_empty(&parent->children) && !parent->data) {
            __btrie_rm_recursive(parent);
        }
    }
}

void*
btrie_remove(struct btrie* root, u32_t index)
{
    struct btrie_node* node = __btrie_traversal(root, index, 0);
    if (!node) {
        return 0;
    }
    void* data = node->data;
    if (!llist_empty(&node->children)) {
        node->data = NULL;
    } else {
        __btrie_rm_recursive(node);
    }
    return data;
}

void
btrie_release(struct btrie* tree)
{
    struct btrie_node *pos, *n;
    llist_for_each(pos, n, &tree->btrie_root->nodes, nodes)
    {
        vfree(pos);
    }

    vfree(tree->btrie_root);
}