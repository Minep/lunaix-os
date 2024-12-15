#include <lunaix/ds/btrie.h>
#include <testing/basic.h>
#include <lunaix/spike.h>
#include <lunaix/compiler.h>

#define WIDTH   8

static void no_inline
__alloc_simple()
{
    struct btrie tree;
    struct btrie_node *root;
    btrie_init(&tree, ilog2(WIDTH));

    expect_long(btrie_map(&tree, 0, 100, (void*)1), 0);
    expect_long(btrie_map(&tree, 0, 100, (void*)2), 1);
    expect_long(btrie_map(&tree, 0, 100, (void*)3), 2);
    expect_long(btrie_map(&tree, 0, 100, (void*)4), 3);

    root = tree.btrie_root;
    expect_notnull(root->children);
    expect_int(root->children_cnt, 4);

    for (int i = 0; i < 4; i++)
    {
        expect_notnull(root->children[i]);
        expect_int(root->children[i]->index, i);
        expect_ulong((ptr_t)root->children[i]->data, i + 1);
    }
    
    for (int i = 4; i < WIDTH; i++)
    {
        expect_null(root->children[i]);
    }

    btrie_release(&tree);
}

static void no_inline
__alloc_edge()
{
    struct btrie tree;
    struct btrie_node *root;
    btrie_init(&tree, ilog2(WIDTH));

    expect_long(btrie_map(&tree, 7, 100, (void*)1), 7);
    expect_long(btrie_map(&tree, 7, 100, (void*)2), 8);
    expect_long(btrie_map(&tree, 7, 100, (void*)3), 9);
    expect_long(btrie_map(&tree, 7, 100, (void*)4), 10);

    root = tree.btrie_root;
    expect_notnull(root->children);
    expect_int(root->children_cnt, 2);

    expect_notnull(root->children[7]);
    expect_int(root->children[7]->index, 7);
    expect_ulong((ptr_t)root->children[7]->data, 1);

    expect_notnull(root->children[1]);
    root = root->children[1];

    for (int i = 0; i < 3; i++)
    {
        expect_notnull(root->children[i]);
        expect_int(root->children[i]->index, i);
        expect_ulong((ptr_t)root->children[i]->data, i + 2);
    }

    btrie_release(&tree);
}

static void no_inline
__alloc_narrow()
{
    struct btrie tree;
    struct btrie_node *root;
    btrie_init(&tree, ilog2(WIDTH));

    expect_long(btrie_map(&tree, 4, 7, (void*)1), 4);
    expect_long(btrie_map(&tree, 4, 7, (void*)2), 5);
    expect_long(btrie_map(&tree, 4, 7, (void*)3), 6);
    expect_long(btrie_map(&tree, 4, 7, (void*)4), -1);

    root = tree.btrie_root;
    expect_notnull(root->children);
    expect_int(root->children_cnt, 3);

    for (int i = 0; i < 4; ++i)
    {
        expect_null(root->children[i]);
    }

    for (int i = 4, j = 1; i < WIDTH - 1; ++i, ++j)
    {
        expect_notnull(root->children[i]);
        expect_int(root->children[i]->index, i);
        expect_ulong((ptr_t)root->children[i]->data, j);
    }

    btrie_release(&tree);
}

static void no_inline
__alloc_narrow_partial()
{
    struct btrie tree;
    struct btrie_node *root;
    btrie_init(&tree, ilog2(WIDTH));

    expect_long(btrie_map(&tree, 15, 17, (void*)1), 15);
    expect_long(btrie_map(&tree, 15, 17, (void*)2), 16);
    expect_long(btrie_map(&tree, 15, 17, (void*)3), -1);

    root = tree.btrie_root;
    expect_notnull(root->children);
    expect_int(root->children_cnt, 2);

    // check left subtree
    
    root = root->children[1];
    expect_notnull(root);
    expect_int(root->children_cnt, 1);

    int i = 0;
    for (; i < WIDTH - 1; ++i)
    {
        expect_null(root->children[i]);
    }

    // i = WIDTH - 1
    expect_notnull(root->children[i]);
    expect_int(root->children[i]->index, i);
    expect_ulong((ptr_t)root->children[i]->data, 1);

    // check right subtree

    root = tree.btrie_root;
    root = root->children[2];
    expect_notnull(root);
    expect_int(root->children_cnt, 1);
    
    i = 0;
    expect_notnull(root->children[i]);
    expect_int(root->children[i]->index, i);
    expect_ulong((ptr_t)root->children[i]->data, 2);

    for (i++; i < WIDTH; ++i)
    {
        expect_null(root->children[i]);
    }

    btrie_release(&tree);
}

static void no_inline
__alloc_dense()
{
    int mis_alloc = 0;
    struct btrie tree;
    btrie_init(&tree, ilog2(WIDTH));

    for (size_t i = 0; i < 1000; i++)
    {
        if (btrie_map(&tree, 0, 1001, (void*)i+1) == -1UL)
        {
            mis_alloc++;
        }
    }
    
    expect_int(mis_alloc, 0);
    btrie_release(&tree);
}

static void no_inline
__alloc_retrive()
{
    struct btrie tree;
    struct btrie_node *root;
    btrie_init(&tree, ilog2(WIDTH));

    expect_long(btrie_map(&tree, 4, 7, (void*)1), 4);
    expect_long(btrie_map(&tree, 4, 7, (void*)2), 5);
    expect_long(btrie_map(&tree, 4, 7, (void*)3), 6);

    expect_ulong(__ptr(btrie_get(&tree, 6)), 3);
    expect_ulong(__ptr(btrie_get(&tree, 5)), 2);
    expect_ulong(__ptr(btrie_get(&tree, 4)), 1);

    btrie_release(&tree);
}

void 
run_test(int argc, const char* argv[])
{
    testcase("simple_alloc", __alloc_simple());
    testcase("simple_edge",  __alloc_edge());
    testcase("simple_narrow",  __alloc_narrow());
    testcase("narrow_partial",  __alloc_narrow_partial());
    testcase("alloc_dense",  __alloc_dense());
    testcase("alloc_get",  __alloc_retrive());
}