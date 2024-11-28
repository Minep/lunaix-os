#include "common.h"
#include <testing/basic.h>

static void
test_fdt_basic_iter(ptr_t dtb)
{
    struct fdt_blob fdt;
    fdt_loc_t loc;
    int depth = 0;

    fdt_load(&fdt, dtb);
    loc = fdt.root;

    testcase("iter-child1", {
        loc = fdt_next_token(loc, &depth);
        
        expect_int(depth, 1);
        expect_int(loc.token->token, FDT_NOD_BEGIN);
        expect_str(loc.node->name, "child@1");
    });

    testcase("iter-child1-prop", {
        loc = fdt_next_token(loc, &depth);
        
        expect_int(depth, 1);
        expect_true(fdt_prop(loc.token));
        expect_str(fdt_prop_key(&fdt, loc), "prop-child1");
    });

    testcase("iter-child1-end", {
        loc = fdt_next_token(loc, &depth);
        
        expect_int(depth, 0);
        expect_true(fdt_node_end(loc.token));
    });

    testcase("iter-step-out", {
        loc = fdt_next_token(loc, &depth);
        
        expect_int(depth, -1);
        expect_true(fdt_node(loc.token));
        expect_str(loc.node->name, "child@2");
    });
}

static void
test_fdt_sibling_iter(ptr_t dtb)
{
    struct fdt_blob fdt;
    fdt_loc_t loc;
    int depth = 0;

    fdt_load(&fdt, dtb);
    loc = fdt.root;

    loc = fdt_descend_into(loc);

    testcase("sibling-iter-1", {
        expect_int(loc.token->token, FDT_NOD_BEGIN);
        expect_str(loc.node->name, "child@1");
    });

    testcase("sibling-iter-2", {
        expect_true(fdt_next_sibling(loc, &loc));

        expect_int(loc.token->token, FDT_NOD_BEGIN);
        expect_str(loc.node->name, "child@2");
    });

    testcase("sibling-iter-3", {
        expect_true(fdt_next_sibling(loc, &loc));

        expect_int(loc.token->token, FDT_NOD_BEGIN);
        expect_str(loc.node->name, "child@3");
    });

    testcase("sibling-iter-3", {
        expect_false(fdt_next_sibling(loc, &loc));

        expect_true(fdt_node_end(loc.token));
    });
}

static void
test_fdt_descend_ascend(ptr_t dtb)
{
    struct fdt_blob fdt;
    fdt_loc_t loc;
    int depth = 0;

    fdt_load(&fdt, dtb);
    loc = fdt.root;

    testcase("descend-to-child1", {
        loc = fdt_descend_into(loc);

        expect_int(loc.token->token, FDT_NOD_BEGIN);
        expect_str(loc.node->name, "child@1");
    });

    testcase("goto-child2", {
        expect_true(fdt_next_sibling(loc, &loc));

        expect_int(loc.token->token, FDT_NOD_BEGIN);
        expect_str(loc.node->name, "child@2");

        loc = fdt_descend_into(loc);
        
        expect_int(loc.token->token, FDT_PROP);
        expect_str(fdt_prop_key(&fdt, loc), "prop-child2");
    });

    testcase("descend-on-prop", {
        loc = fdt_descend_into(loc);

        expect_true(loc.ptr == loc.ptr);
        expect_int(loc.token->token, FDT_PROP);
        expect_str(fdt_prop_key(&fdt, loc), "prop-child2");
    });

    testcase("descend-to-child21", {
        expect_true(fdt_next_sibling(loc, &loc));

        expect_int(loc.token->token, FDT_NOD_BEGIN);
        expect_str(loc.node->name, "child@21");
    });

    testcase("ascend", {
        loc = fdt_ascend_from(loc);

        expect_int(loc.token->token, FDT_NOD_BEGIN);
        expect_str(loc.node->name, "child@3");
    });

    testcase("ascend-to-root", {
        loc = fdt_ascend_from(loc);

        expect_true(fdt_node_end(loc.token));
    });
}


static void
test_find_prop(ptr_t dtb)
{
    struct fdt_blob fdt;
    fdt_loc_t loc;
    struct dtp_val val;
    int depth = 0;

    fdt_load(&fdt, dtb);
    loc = fdt.root;

    testcase("prop-find-child1", {
        loc = fdt_descend_into(loc);
        expect_int(loc.token->token, FDT_NOD_BEGIN);
        expect_str(loc.node->name, "child@1");

        expect_true(fdt_find_prop(&fdt, loc, "prop-child1", &val));
        expect_int(val.size, 0);

        expect_false(fdt_find_prop(&fdt, loc, "prop-child2", &val));
    });
}

void
run_test(int argc, const char* argv[])
{
    ptr_t dtb = load_fdt();

    test_fdt_basic_iter(dtb);
    test_fdt_descend_ascend(dtb);
    test_fdt_sibling_iter(dtb);
    test_find_prop(dtb);
}