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

enum btrie_query_mode {
    BTRIE_FIND,
    BTRIE_FIND_PARENT,
    BTRIE_INSERT
};

#define remove_visit_flag(node)     ((struct btrie_node*)(__ptr(node) & ~1))

static inline void
__btrie_addchild(struct btrie* root,
                 struct btrie_node* parent, struct btrie_node* child)
{
    if (unlikely(!parent->children)) {
        parent->children = vcalloc(sizeof(parent), 1 << root->order);
    }

    parent->children_cnt++;
    parent->children[child->index] = child;
}

static inline struct btrie_node*
__btrie_get_child(struct btrie_node* node, int index)
{
    if (unlikely(!node || !node->children)) {
        return NULL;
    }

    return remove_visit_flag(node->children[index]);
}

static inline bool
__btrie_child_visited(struct btrie_node* node, int index)
{
    if (unlikely(!node || !node->children)) {
        return false;
    }

    return !!(__ptr(node->children[index]) & 1);
}

static inline void
__btrie_visit_child(struct btrie_node* node, int index)
{
    if (unlikely(!node || !node->children)) {
        return;
    }

    node->children[index] 
            = (struct btrie_node*)(__ptr(node->children[index]) | 1); 
}

static inline void
__btrie_unvisit_child(struct btrie_node* node, int index)
{
    if (unlikely(!node || !node->children)) {
        return;
    }

    node->children[index] = remove_visit_flag(node->children[index]); 
}

static inline void
__btrie_free_node(struct btrie_node* node)
{
    vfree_safe(node->children);
    vfree(node);
}

static inline bool
__full_node(struct btrie* root, struct btrie_node* node)
{
    return node->children_cnt == 1 << root->order;
}

static inline struct btrie_node*
__btrie_create(struct btrie* root, struct btrie_node* parent, int index)
{
    struct btrie_node* node;

    node = vzalloc(sizeof(struct btrie_node));
    node->index = index;
    node->parent = parent;
    
    if (likely(parent)) {
        llist_append(&root->btrie_root->nodes, &node->nodes);
        __btrie_addchild(root, parent, node);
    }

    return node;
}

static struct btrie_node*
__btrie_traversal(struct btrie* root, 
                  unsigned long key, enum btrie_query_mode mode)
{
    unsigned long lz, bitmask, i = 0;
    struct btrie_node *subtree, *tree = root->btrie_root;
    
    lz = !key ? 0 : (msbitl - clzl(key)) / root->order;
    lz = lz * root->order;
    bitmask = ((1 << root->order) - 1) << lz;

    // Time complexity: O(log_2(log_2(N))) where N is the key to lookup
    while (bitmask && tree) {
        i = (key & bitmask) >> lz;
        subtree = __btrie_get_child(tree, i);

        if (!lz && mode == BTRIE_FIND_PARENT) {
            break;
        }

        if (!subtree && (mode != BTRIE_FIND)) {
            tree = __btrie_create(root, tree, i);
        }
        else {
            tree = subtree;
        }

        bitmask = bitmask >> root->order;
        lz -= root->order;
    }

    return tree;
}

#define check_partial(loc, start, end, mask)    \
        (((long)(start) - (long)mask - 1) <= (long)loc && loc < end)

static inline struct btrie_node*
__get_immediate_node(struct btrie* tree, struct btrie_node *root, 
                     unsigned long start, unsigned long end, unsigned long loc)
{
    unsigned int index;
    unsigned long mask;
    struct btrie_node *node;

    mask  = (1UL << tree->order) - 1;
    index = loc & mask;
    for (int i = 0; i <= mask; ++i, index = (index + 1) & mask) 
    {    
        loc = (loc & ~mask) + index;
        node = __btrie_get_child(root, index);

        if (loc < start || loc >= end) {
            continue;
        }

        if (!node) {
            return __btrie_create(tree, root, index);
        }

        if (!node->data) {
            return node;
        }
    }

    return NULL;
}

static struct btrie_node*
__btrie_find_free(struct btrie* tree, struct btrie_node *root, 
                  unsigned long start, unsigned long end, unsigned long loc)
{
    unsigned long mask, next_loc;
    struct btrie_node *node, *found;

    if (!root) return NULL;

    mask  = (1UL << tree->order) - 1;

    __btrie_visit_child(root->parent, root->index);
    
    if (!__full_node(tree, root)) {
        found = __get_immediate_node(tree, root, start, end, loc);
        if (found) goto done;
    }

    for (unsigned int i = 0; i <= mask; i++)
    {
        next_loc = ((loc & ~mask) + i) << tree->order;

        if (!next_loc || !check_partial(next_loc, start, end, mask)) {
            continue;
        }

        if (__btrie_child_visited(root, i)) {
            continue;
        }

        node = __btrie_get_child(root, i);
        node = node ?: __btrie_create(tree, root, i);

        found = __btrie_find_free(tree, node, start, end, next_loc);

        if (found) {
            goto done;
        }
    }

    loc >>= tree->order;
    found = __btrie_find_free(tree, root->parent, start, end, loc + 1);

done:
    __btrie_unvisit_child(root->parent, root->index);
    return found;
}

static unsigned long
__btrie_alloc_slot(struct btrie* tree, struct btrie_node **slot, 
                   unsigned long start, unsigned long end)
{
    unsigned int od;
    unsigned long result, mask;
    struct btrie_node *found, *node;
    
    od   = 0;
    mask = (1 << od) - 1;
    found = tree->btrie_root;
    result = 0;

    if (!start && !__btrie_get_child(found, 0)) {
        *slot = __btrie_create(tree, found, 0);
        return 0;
    }

    found = __btrie_traversal(tree, start, BTRIE_FIND_PARENT);
    found = __btrie_find_free(tree, found, start, end, start);
    
    node = found;
    while (node) 
    {
        result |= node->index << od;
        od += tree->order;
        node = node->parent;
    }

    *slot = found;
    return found ? result : -1UL;
}

void
btrie_init(struct btrie* btrie, unsigned int order)
{
    btrie->btrie_root = __btrie_create(btrie, NULL, 0);
    btrie->order = order;

    llist_init_head(&btrie->btrie_root->nodes);
}

void*
btrie_get(struct btrie* root, unsigned long index)
{
    struct btrie_node* node;

    node = __btrie_traversal(root, index, BTRIE_FIND);
    if (!node) {
        return NULL;
    }

    return node->data;
}

void
btrie_set(struct btrie* root, unsigned long index, void* data)
{
    struct btrie_node* node;

    node = __btrie_traversal(root, index, BTRIE_INSERT);
    node->data = data;
}

void
__btrie_rm_recursive(struct btrie_node* node)
{
    struct btrie_node* parent = node->parent;

    if (!parent) {
        return;
    }

    parent->children[node->index] = NULL;
    parent->children_cnt--;
    __btrie_free_node(node);

    if (!parent->children_cnt && !parent->data) {
        __btrie_rm_recursive(parent);
    }
}

unsigned long
btrie_map(struct btrie* root, 
          unsigned long start, unsigned long end, void* data)
{
    struct btrie_node* node = NULL;
    unsigned long alloced;

    alloced = __btrie_alloc_slot(root, &node, start, end);
    
    if (!node)
        return -1;

    node->data = data;
    return alloced;
}

void*
btrie_remove(struct btrie* root, unsigned long index)
{
    void* data;
    struct btrie_node* node;

    node = __btrie_traversal(root, index, BTRIE_FIND);
    if (!node) {
        return 0;
    }
    
    data = node->data;
    if (!node->children_cnt) {
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
        __btrie_free_node(pos);
    }

    __btrie_free_node(tree->btrie_root);
}