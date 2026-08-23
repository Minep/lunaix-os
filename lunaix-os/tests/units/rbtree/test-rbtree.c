#include "lunaix/spike.h"
#include <lunaix/ds/rbtree.h>
#include <testing/basic.h>
#include <lunaix/mm/valloc.h>

struct node {
    struct rbnode node;
    int key;
};

struct tree {
    struct node* root;
};

#define node_of(rbnode) \
    container_of(rbnode, struct node, node)

static inline struct node*
__alloc_node(int key)
{
    struct node* node;

    node = valloc(sizeof(*node));

    node->key = key;
    return node;
}

static struct node*
__find_slot(struct node* root, int key, struct rbnode*** side)
{
    struct rbnode** next;

    if (key > root->key) {
        next = &root->node.right;
    }
    else {
        next = &root->node.left;
    }

    if (!*next) {
        *side = next;
        return root;
    }

    return __find_slot(node_of(*next), key, side);
}

static void
__tear_down(struct rbnode* root)
{
    if (!root)
        return;

    __tear_down(root->right);
    __tear_down(root->left);
    vfree(node_of(root));
}

static int
__h(struct rbnode* root)
{
    if (!root)
        return 0;

    return 1 + MAX(__h(root->right), __h(root->left));
}

static int
__factor(struct rbnode* root)
{
    return __h(root->right) - __h(root->left);
}

static void
__insert(struct tree* tree, int key)
{
    struct rbnode* new_node, **side;
    struct node *parent, *this;
    
    side = NULL;
    this = __alloc_node(key);

    if (!tree->root) {
        tree->root = this;
        rbtree_fixup_insert(&tree->root->node, NULL, &this->node);
        return;
    }

    parent = __find_slot(tree->root, key, &side);

    expect_notnull(parent);
    expect_notnull(side);

    *side = &this->node;

    new_node = rbtree_fixup_insert(&tree->root->node, &parent->node, &this->node);
    tree->root = node_of(new_node);
}

static void
__insert_batch(struct tree* tree, int* arr, int len)
{
    for (int i = 0; i < len; i++) {
        __insert(tree, arr[i]);
    }
}

static no_inline void
test_ordered_1()
{
    struct tree T = {};
    int bias;
    int arr[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8
    };

    __insert_batch(&T, arr, 9);

    bias = __factor(&T.root->node);
    expect_int(bias, 1);

    __tear_down(&T.root->node);
}

static no_inline void
test_ordered_2()
{
    struct tree T = {};
    int bias;
    int arr[] = {
        0, 1, 2, 3, 4, 5, 6, 7
    };

    __insert_batch(&T, arr, 8);

    bias = __factor(&T.root->node);
    expect_int(bias, 1);
    
    __tear_down(&T.root->node);
}

static no_inline void
test_ordered_3()
{
    struct tree T = {};
    int bias;
    int arr[] = {
        1, 5, 2, 4, 3, 6, 8, 7, 0
    };

    __insert_batch(&T, arr, 9);

    bias = __factor(&T.root->node);
    expect_int(bias, 0);
    
    __tear_down(&T.root->node);
}

static no_inline void
test_ordered_4()
{
    struct tree T = {};
    int bias;
    int arr[] = {
        8, 7, 6, 5, 4, 3, 2, 1, 0
    };

    __insert_batch(&T, arr, 9);

    bias = __factor(&T.root->node);
    expect_int(bias, -1);
    
    __tear_down(&T.root->node);
}

void 
run_test(int argc, const char* argv[])
{
    testcase("insert_ordered_in_order", test_ordered_1());
    testcase("insert_ordered_in_order_inbalance", test_ordered_2());
    testcase("insert_ordered_out_of_order", test_ordered_3());
    testcase("insert_ordered_reverse_order", test_ordered_4());
}
