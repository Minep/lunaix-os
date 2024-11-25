#include "common.h"
#include <testing/basic.h>
#include <unistd.h>

static inline struct dtn* 
get_node(struct dtn* parent, const char* name)
{
    struct dtn_iter it;
    struct dtn* node;
    
    dt_begin_find_byname(&it, parent, name);
    
    node = (struct dtn*)it.matched;
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
    struct dtn* node;

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