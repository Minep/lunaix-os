#ifndef __LUNAIX_DS_RBTREE_H__
#define __LUNAIX_DS_RBTREE_H__

#include <lunaix/types.h>


/**
 * A red-black tree node.
 *
 * This implementation interpretes the red-black
 * tree as a generic self-balancing tree structure, rather
 * than a strict textbook style binary search tree.
 *
 * However, the algorithm for balancing is still following
 * the BST ordering invariant assumption. Thus using it as 
 * a BST is totally expected, as long as you follows the 
 * ordering where LHS always smaller than RHS.
 *
 * Due to this reason, user should bring their own insertion
 * implementation, and use `rbtree_fixup_insert` after insertion.
 *
 */
struct rbnode {
    ptr_t parent_val;
    struct rbnode* left;
    struct rbnode* right;
};

static inline struct rbnode* 
rbnode_parent(const struct rbnode* node)
{
    return (struct rbnode*)(node->parent_val & ~1);
}

/**
 * Rebalance the tree identified by root. 
 * Return the new root after rebalancing
 */
struct rbnode*
rbtree_fixup_insert(struct rbnode* root, 
                    struct rbnode* parent, struct rbnode* node);


/**
 * Delete the node from the tree identified by root. 
 * Return the new root after deletion + rebalancing
 */
struct rbnode*
rbtree_delete(struct rbnode* root, struct rbnode* node);

#endif
