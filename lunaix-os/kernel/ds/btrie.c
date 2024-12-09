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

enum btrie_traverse_mode {
    BTRIE_FIND,
    BTRIE_FIND_PARENT,
    BTRIE_INSERT
};
struct btrie_locator
{
    enum btrie_traverse_mode mode;
    int shifts;
    unsigned long index;
};

static inline void
__btrie_locator(struct btrie_locator* loc, 
                unsigned long index, enum btrie_traverse_mode mode)
{
    loc->mode = mode;
    loc->index = index;
}

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
    if (unlikely(!node->children)) {
        return NULL;
    }

    return node->children[index];
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
__btrie_traversal(struct btrie* root, struct btrie_locator* locator)
{
    unsigned long lz, index;
    unsigned long bitmask;
    unsigned long i = 0;
    struct btrie_node *subtree, *tree = root->btrie_root;

    index = locator->index;
    lz = index ? (msbitl - clzl(index)) / root->order : 0;
    lz = lz * root->order;
    bitmask = ((1 << root->order) - 1) << lz;

    // Time complexity: O(log_2(log_2(N))) where N is the index to lookup
    while (bitmask && tree) {
        i = (index & bitmask) >> lz;
        subtree = __btrie_get_child(tree, i);

        if (!subtree && (locator->mode != BTRIE_FIND)) 
        {
            tree = __btrie_create(root, tree, i);
        }
        else {
            tree = subtree;
        }

        if (lz == root->order && locator->mode == BTRIE_FIND_PARENT)
        {
            break;
        }

        bitmask = bitmask >> root->order;
        lz -= root->order;
    }

    locator->shifts = lz;
    return tree;
}

static unsigned long
__btrie_alloc_slot(struct btrie* root, struct btrie_node **slot, 
                   unsigned long start, unsigned long end)
{
    unsigned int index, od;
    unsigned long result, mask, result_next;
    struct btrie_node *tree, *subtree;
    struct btrie_locator locator;
    
    result = 0;
    od   = root->order;
    mask = (1 << od) - 1;
    tree = root->btrie_root;
    
    if (start) {
        __btrie_locator(&locator, start, BTRIE_FIND_PARENT);
        tree = __btrie_traversal(root, &locator);
        result = start;
    }
    
    while (tree && result < end) 
    {
        index = result & mask;
        result_next = result << od;
        
        subtree = __btrie_get_child(tree, index);
            
        if (subtree) {
            if (result >= start && !subtree->data) {
                tree = subtree;
                goto done;
            }
        }
        else {
            subtree = __btrie_create(root, tree, index);

            if (result >= start)
                goto done;

            goto descend;
        }

        if (!__full_node(root, tree) && index != mask) {
            result++;
            continue;
        }

descend:
        if (result_next >= end) {
            tree = tree->parent;
            result >>= od;
            result++;
            continue;
        }

        tree   = subtree;
        result = result_next;
    }

    *slot = NULL;
    return -1;

done:
    *slot = subtree;
    return result;
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
    struct btrie_locator locator;
    
    __btrie_locator(&locator, index, BTRIE_FIND);
    node = __btrie_traversal(root, &locator);
    if (!node) {
        return NULL;
    }

    return node->data;
}

void
btrie_set(struct btrie* root, unsigned long index, void* data)
{
    struct btrie_node* node;
    struct btrie_locator locator;
    
    __btrie_locator(&locator, index, BTRIE_INSERT);
    node = __btrie_traversal(root, &locator);
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
    struct btrie_locator locator;
    
    __btrie_locator(&locator, index, BTRIE_FIND);
    node = __btrie_traversal(root, &locator);
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