#include "common.h"
#include <testing/basic.h>
#include <unistd.h>

static inline struct dt_node* 
get_node(struct dt_node* parent, const char* name)
{
    struct dt_node_iter it;
    struct dt_node* node;
    
    dt_begin_find_byname(&it, parent, name);
    
    node = (struct dt_node*)it.matched;
    expect_notnull(node);

    return node;
}


void
run_test(int argc, const char* argv[])
{

    if(!my_load_dtb()) {
        printf("failed to load dtb\n");
        _exit(1);
    }

    struct dt_context* ctx;
    struct dt_node* node;

    ctx = dt_main_context();

    begin_testcase("root");

    node = ctx->root;
    expect_int(node->base.addr_c, 1);
    expect_int(node->base.sz_c, 2);
    expect_int(node->base.intr_c, 0);
    end_testcase();

    begin_testcase("level-1 child");

    node = get_node(node, "child@1");
    expect_int(node->base.addr_c, 1);
    expect_int(node->base.sz_c, 2);
    expect_int(node->base.intr_c, 3);
    end_testcase();

    begin_testcase("level-2 child");

    node = get_node(node, "child@2");
    expect_int(node->base.addr_c, 4);
    expect_int(node->base.sz_c, 2);
    expect_int(node->base.intr_c, 3);
    end_testcase();

    begin_testcase("level-3 child");

    node = get_node(node, "child@3");
    expect_int(node->base.addr_c, 4);
    expect_int(node->base.sz_c, 0);
    expect_int(node->base.intr_c, 0);
    end_testcase();

}