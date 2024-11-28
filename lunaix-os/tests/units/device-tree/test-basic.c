#include "common.h"
#include <testing/basic.h>
#include <unistd.h>

static inline struct dtn* 
validate_node(struct dt_context* ctx, 
              const char* name, const char* compat)
{
    struct dtn_iter it;
    struct dtn* node = ctx->root;

    
    dt_begin_find_byname(&it, node, name);
    
    node = (struct dtn*)it.matched;

    expect_notnull(node);
    expect_str(node->base.compat.str_val, compat);
    expect_int(node->base.addr_c, 1);
    expect_int(node->base.sz_c, 2);

    return node;
}

static void
testcase_rootfield(struct dt_context* ctx)
{
    struct dtn* node = ctx->root;

    begin_testcase("root");

    expect_str(node->base.compat.str_val, "root");
    expect_int(node->base.addr_c, 1);
    expect_int(node->base.sz_c, 2);

    end_testcase();
}

static void
testcase_child1(struct dt_context* ctx)
{
    struct dtn* node;
    struct dtp_val* val;

    begin_testcase("trivial-props");
    
    node = validate_node(ctx, "child@1", "child,simple-fields");

    val = dt_getprop(&node->base, "field-str");
    expect_notnull(val);
    expect_str(val->str_val, "field");

    val = dt_getprop(&node->base, "field-u32");
    expect_notnull(val);
    expect_int(dtp_u32(val), 32);

    val = dt_getprop(&node->base, "field-u64");
    expect_notnull(val);
    expect_ulong(dtp_u64(val), (0xfUL << 32 | 0x1));

    val = dt_getprop(&node->base, "field-strlst");
    expect_notnull(val);

    char* str;
    int i = 0;
    const char *expected[] = {"str1", "str2", "str3"};
    dtprop_strlst_foreach(str, val) {
        expect_str(str, expected[i]);
        i++;
    }

    end_testcase();
}

static void
testcase_child2(struct dt_context* ctx)
{
    struct dtn* node;
    struct dtpropx propx;
    struct dtprop_xval val;
    struct dtp_val* prop;
    
    begin_testcase("dtpx-reg");
    
    node = validate_node(ctx, "child@2", "child,encoded-array");

    dt_proplet reg = { dtprop_u32, dtprop_u64, dtprop_end };

    dtpx_compile_proplet(reg);
    dtpx_prepare_with(&propx, &node->reg, reg);
    
    expect_true(dtpx_extract_at(&propx, &val, 0));
    expect_int(val.u32, 0x12345);

    expect_true(dtpx_extract_at(&propx, &val, 1));
    expect_ulong(val.u64, (0x1UL << 32) | 0x2UL);

    expect_false(dtpx_extract_at(&propx, &val, 2));
    expect_false(dtpx_goto_row(&propx, 1));

    end_testcase();
    

    begin_testcase("dtpx-mixed");

    struct dtprop_xval xvals[5];
    dt_proplet mixed_array = { 
        dtprop_handle, dtprop_u32, dtprop_u32, dtprop_u32, dtprop_u32,
        dtprop_end
    };

    prop = dt_getprop(&node->base, "mixed_array");
    expect_notnull(prop);

    dtpx_compile_proplet(mixed_array);
    dtpx_prepare_with(&propx, prop, mixed_array);

    expect_true(dtpx_extract_row(&propx, xvals, 5));
    
    expect_notnull(xvals[0].phandle);
    expect_str(morpher_name(dt_mobj(xvals[0].phandle)), "child@1");
    expect_int(xvals[1].u32, 1);
    expect_int(xvals[2].u32, 3);
    expect_int(xvals[3].u32, 5);
    expect_int(xvals[4].u32, 7);

    expect_true(dtpx_next_row(&propx));
    expect_true(dtpx_extract_row(&propx, xvals, 5));

    expect_notnull(xvals[0].phandle);
    expect_str(morpher_name(dt_mobj(xvals[0].phandle)), "child@3");
    expect_int(xvals[1].u32, 2);
    expect_int(xvals[2].u32, 4);
    expect_int(xvals[3].u32, 6);
    expect_int(xvals[4].u32, 8);

    end_testcase();
}

static void
testcase_child3(struct dt_context* ctx)
{
    struct dtn* node;
    struct dtpropx propx;
    struct dtprop_xval val;
    struct dtp_val* prop;
    
    begin_testcase("simple-flags");
    
    node = validate_node(ctx, "child@3", "child,flags");

    expect_true(node->base.dma_coherent);
    expect_true(node->base.intr_controll);
    expect_int(node->base.status, STATUS_DISABLE);

    end_testcase();
}

void
run_test(int argc, const char* argv[])
{

    if(!my_load_dtb()) {
        printf("failed to load dtb\n");
        _exit(1);
    }

    struct dt_context* ctx;
    ctx = dt_main_context();

    testcase_rootfield(ctx);
    testcase_child1(ctx);
    testcase_child3(ctx);
    testcase_child2(ctx);
}