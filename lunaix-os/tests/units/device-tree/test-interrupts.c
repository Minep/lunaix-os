#include "common.h"
#include <testing/basic.h>

extern void *calloc(size_t, size_t);
extern void  free(void *);

void
run_test(int argc, const char* argv[])
{
    struct dt_context* ctx;
    struct dtn* node, *domain;
    struct dtn_iter it;
    struct dtp_val val;
    
    my_load_dtb();

    ctx = dt_main_context();

    node = ctx->root;
    testcase("root", {
        expect_notnull(node);
        expect_int(node->base.addr_c, 0);
        expect_int(node->base.sz_c, 0);
        expect_int(node->base.intr_c, 0);
    });

    dt_begin_find_byname(&it, ctx->root, "dev@1");
    node = (struct dtn*)it.matched;
    
    testcase("intr-basic", {
        expect_notnull(node);
        expect_notnull(node->intr.parent);

        expect_str(dt_name(node), "dev@1");
        expect_str(dt_name(node->intr.parent), "pic@11");
    });

    testcase("intr-single", {
        expect_int(node->intr.nr_intrs, 1);
        expect_false(node->intr.extended);

        domain = dt_interrupt_at(node, 0, &val);
        expect_int(val.ref->raw[0], 1);
        expect_int(val.ref->raw[1], 2);
        expect_str(dt_name(domain), "pic@11");

        expect_null(dt_interrupt_at(node, 1, &val));
    });

    dt_begin_find_byname(&it, ctx->root, "dev@2");
    node = (struct dtn*)it.matched;

    testcase("intr-multi", {
        expect_notnull(node);
        expect_int(node->intr.nr_intrs, 3);
        expect_false(node->intr.extended);

        domain = dt_interrupt_at(node, 0, &val);
        expect_int(val.ref->raw[0], 1);
        expect_int(val.ref->raw[1], 1);
        expect_str(dt_name(domain), "pic@11");

        domain = dt_interrupt_at(node, 1, &val);
        expect_int(val.ref->raw[0], 1);
        expect_int(val.ref->raw[1], 2);
        expect_str(dt_name(domain), "pic@11");

        domain = dt_interrupt_at(node, 2, &val);
        expect_int(val.ref->raw[0], 2);
        expect_int(val.ref->raw[1], 2);
        expect_str(dt_name(domain), "pic@11");

        expect_null(dt_interrupt_at(node, 3, &val));
    });

    dt_begin_find_byname(&it, ctx->root, "dev@3");
    node = (struct dtn*)it.matched;

    testcase("intr-ext-multi", {
        expect_notnull(node);

        expect_int(node->intr.nr_intrs, 3);
        expect_true(node->intr.extended);

        domain = dt_interrupt_at(node, 0, &val);
        expect_int(val.size / sizeof(int), 2);
        expect_int(val.ref->raw[0], 3);
        expect_int(val.ref->raw[1], 3);
        expect_str(dt_name(domain), "pic@11");

        domain = dt_interrupt_at(node, 1, &val);
        expect_int(val.size / sizeof(int), 1);
        expect_int(val.ref->raw[0], 1);
        expect_str(dt_name(domain), "pic@22");

        domain = dt_interrupt_at(node, 2, &val);
        expect_int(val.size / sizeof(int), 4);
        expect_int(val.ref->raw[0], 1);
        expect_int(val.ref->raw[1], 2);
        expect_int(val.ref->raw[2], 3);
        expect_int(val.ref->raw[3], 4);
        expect_str(dt_name(domain), "pic@33");

        expect_null(dt_interrupt_at(node, 3, &val));
    });
}