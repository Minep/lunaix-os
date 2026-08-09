#include <lunaix/ds/rbtree.h>

#define __rb_parent(x) rbnode_parent(x)

enum rbnode_color {
    RB_BLACK,
    RB_RED
};

static inline enum rbnode_color
__rb_color(const struct rbnode* node)
{
    return ((node->parent_val) & 1) ? RB_BLACK : RB_RED;
}

static inline void
__rb_setparent(struct rbnode* node, struct rbnode* parent)
{
    node->parent_val = 
        ((ptr_t)parent & ~1UL) | (node->parent_val & 1);
}

static inline void
__rb_setcolor(struct rbnode* node, enum rbnode_color color)
{
    node->parent_val = 
        (node->parent_val & ~1UL) | (color == RB_BLACK ? 1 : 0);
}
static inline int
rb_is_red(const struct rbnode *node)
{
    return node != NULL && __rb_color(node) == RB_RED;
}

static inline int
rb_is_black(const struct rbnode *node)
{
    return !rb_is_red(node);
}

static struct rbnode *
__rb_rotate_left(struct rbnode *root, struct rbnode *x)
{
    struct rbnode *y = x->right;
    struct rbnode *p = __rb_parent(x);

    x->right = y->left;
    if (y->left)
        __rb_setparent(y->left, x);

    y->left = x;
    __rb_setparent(x, y);
    __rb_setparent(y, p);

    if (!p)
        root = y;
    else if (p->left == x)
        p->left = y;
    else
        p->right = y;

    return root;
}

static struct rbnode *
__rb_rotate_right(struct rbnode *root, struct rbnode *x)
{
    struct rbnode *y = x->left;
    struct rbnode *p = __rb_parent(x);

    x->left = y->right;
    if (y->right)
        __rb_setparent(y->right, x);

    y->right = x;
    __rb_setparent(x, y);
    __rb_setparent(y, p);

    if (!p)
        root = y;
    else if (p->left == x)
        p->left = y;
    else
        p->right = y;

    return root;
}

static struct rbnode *
__rb_delete_fixup(struct rbnode *root, struct rbnode *node,
                  struct rbnode *parent)
{
    struct rbnode *sibling;

    while (node != root && rb_is_black(node)) {
        if (parent->left == node) {
            sibling = parent->right;

            /* Case 1: red sibling -> rotate into a black-sibling case. */
            if (rb_is_red(sibling)) {
                __rb_setcolor(sibling, RB_BLACK);
                __rb_setcolor(parent, RB_RED);
                root = __rb_rotate_left(root, parent);
                sibling = parent->right;
            }

            /*
             * The doubly-black node has black height >= 1, so a black
             * sibling subtree cannot be empty here.
             */
            if (rb_is_black(sibling->left) && rb_is_black(sibling->right)) {
                /* Case 2: push the deficit up to the parent. */
                __rb_setcolor(sibling, RB_RED);
                node   = parent;
                parent = __rb_parent(node);
            } else {
                if (rb_is_black(sibling->right)) {
                    /* Case 3: rotate the red child to the far side. */
                    if (sibling->left)
                        __rb_setcolor(sibling->left, RB_BLACK);
                    __rb_setcolor(sibling, RB_RED);
                    root = __rb_rotate_right(root, sibling);
                    sibling = parent->right;
                }

                /* Case 4: terminal -- one rotation absorbs the deficit. */
                __rb_setcolor(sibling, __rb_color(parent));
                __rb_setcolor(parent, RB_BLACK);
                if (sibling->right)
                    __rb_setcolor(sibling->right, RB_BLACK);
                root = __rb_rotate_left(root, parent);
                node = root;
                break;
            }
        } else {
            sibling = parent->left;

            if (rb_is_red(sibling)) {
                __rb_setcolor(sibling, RB_BLACK);
                __rb_setcolor(parent, RB_RED);
                root = __rb_rotate_right(root, parent);
                sibling = parent->left;
            }

            if (rb_is_black(sibling->left) && rb_is_black(sibling->right)) {
                __rb_setcolor(sibling, RB_RED);
                node   = parent;
                parent = __rb_parent(node);
            } else {
                if (rb_is_black(sibling->left)) {
                    if (sibling->right)
                        __rb_setcolor(sibling->right, RB_BLACK);
                    __rb_setcolor(sibling, RB_RED);
                    root = __rb_rotate_left(root, sibling);
                    sibling = parent->left;
                }

                __rb_setcolor(sibling, __rb_color(parent));
                __rb_setcolor(parent, RB_BLACK);
                if (sibling->left)
                    __rb_setcolor(sibling->left, RB_BLACK);
                root = __rb_rotate_right(root, parent);
                node = root;
                break;
            }
        }
    }

    if (node)
        __rb_setcolor(node, RB_BLACK);

    return root;
}

struct rbnode *
rbtree_delete(struct rbnode *root, struct rbnode *node)
{
    struct rbnode *child, *parent;
    enum rbnode_color removed_color;

    if (!root || !node)
        return root;

    if (node->left && node->right) {
        /*
         * Two children: splice in the in-order successor.
         *
         * The successor is *relinked*, not copied over `node`. The
         * caller owns the enclosing struct and may hold pointers into
         * it, so the node identity must be preserved -- and with no key
         * field there is nothing to copy anyway.
         */
        struct rbnode *succ = node->right;
        struct rbnode *node_parent;

        while (succ->left)
            succ = succ->left;

        /* succ has no left child by construction. */
        child         = succ->right;
        parent        = __rb_parent(succ);
        removed_color = __rb_color(succ);

        if (parent == node) {
            /* succ is node's direct right child; child stays put. */
            parent = succ;
        } else {
            if (child)
                __rb_setparent(child, parent);
            parent->left = child;

            succ->right = node->right;
            __rb_setparent(node->right, succ);
        }

        succ->left = node->left;
        __rb_setparent(node->left, succ);

        /* succ inherits node's position and colour wholesale. */
        __rb_setcolor(succ, __rb_color(node));
        node_parent = __rb_parent(node);
        __rb_setparent(succ, node_parent);

        if (!node_parent)
            root = succ;
        else if (node_parent->left == node)
            node_parent->left = succ;
        else
            node_parent->right = succ;
    } else {
        /* At most one child: promote it directly. */
        child         = node->left ? node->left : node->right;
        parent        = __rb_parent(node);
        removed_color = __rb_color(node);

        if (child)
            __rb_setparent(child, parent);

        if (!parent)
            root = child;
        else if (parent->left == node)
            parent->left = child;
        else
            parent->right = child;
    }

    /*
     * Removing a red node never changes any path's black height, so the
     * fixup is only needed when a black node left the tree.
     */
    if (removed_color == RB_BLACK)
        root = __rb_delete_fixup(root, child, parent);

    /* Leave the detached node in an obviously-unlinked state. */
    node->parent_val = 0;
    node->left       = NULL;
    node->right      = NULL;

    return root;
}


static struct rbnode *
__rbtree_balance(struct rbnode *root, struct rbnode *node)
{
    struct rbnode *parent;

    __rb_setcolor(node, RB_RED);

    while ((parent = __rb_parent(node)) != NULL && rb_is_red(parent)) {
        struct rbnode *grand = __rb_parent(parent);
        struct rbnode *uncle;

        if (!grand)
            break;

        if (grand->left == parent) {
            uncle = grand->right;

            if (rb_is_red(uncle)) {
                /* Recolour and continue upward. */
                __rb_setcolor(parent, RB_BLACK);
                __rb_setcolor(uncle, RB_BLACK);
                __rb_setcolor(grand, RB_RED);
                node = grand;
                continue;
            }

            if (parent->right == node) {
                root = __rb_rotate_left(root, parent);
                node   = parent;
                parent = __rb_parent(node);
            }

            __rb_setcolor(parent, RB_BLACK);
            __rb_setcolor(grand, RB_RED);
            root = __rb_rotate_right(root, grand);
        } else {
            uncle = grand->left;

            if (rb_is_red(uncle)) {
                __rb_setcolor(parent, RB_BLACK);
                __rb_setcolor(uncle, RB_BLACK);
                __rb_setcolor(grand, RB_RED);
                node = grand;
                continue;
            }

            if (parent->left == node) {
                root = __rb_rotate_right(root, parent);
                node   = parent;
                parent = __rb_parent(node);
            }

            __rb_setcolor(parent, RB_BLACK);
            __rb_setcolor(grand, RB_RED);
            root = __rb_rotate_left(root, grand);
        }
    }

    __rb_setcolor(root, RB_BLACK);

    return root;
}

struct rbnode*
rbtree_fixup_insert(struct rbnode* root, 
                    struct rbnode* parent, struct rbnode* node)
{
    __rb_setcolor(node, RB_RED);
    __rb_setparent(node, parent);

    node->left = NULL;
    node->right = NULL;

    return __rbtree_balance(root, node);
}
