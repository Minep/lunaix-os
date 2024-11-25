#include "common.h"
#include <testing/basic.h>

extern void *calloc(size_t, size_t);

#define be(v) ((((v) & 0x000000ff) << 24)  |\
               (((v) & 0x00ff0000) >> 8 )  |\
               (((v) & 0x0000ff00) << 8 )  |\
               (((v) & 0xff000000) >> 24))

static void
map_and_mask_test(struct dtn* node)
{
    struct dt_intr_node* intrn;
    struct dt_intr_map *map;
    struct dt_speckey *mask, *key, test_key;

    intrn = &node->intr;
    map = intrn->map;
    mask = &map->key_mask;

    test_key.val = calloc(4, sizeof(int));
    test_key.bval->raw[0] = 0xf200;
    test_key.bval->raw[1] = 0x22;
    test_key.bval->raw[2] = 0x0;
    test_key.bval->raw[3] = 0x16;

    testcase("map", { expect_notnull(map); });

    testcase("mask-value", {
        expect_int(mask->bval->raw[0], 0xf800);
        expect_int(mask->bval->raw[1], 0);
        expect_int(mask->bval->raw[2], 0);
        expect_int(mask->bval->raw[3], 7);
    });

    testcase("mask-ops", {
        dt_speckey_mask(&test_key, mask);

        expect_int(test_key.bval->raw[0], 0xf000);
        expect_int(test_key.bval->raw[1], 0);
        expect_int(test_key.bval->raw[2], 0);
        expect_int(test_key.bval->raw[3], 6);
    });

    testcase("map-get", {
        
    });

}

void
run_test(int argc, const char* argv[])
{
    struct dt_context* ctx;
    struct dtn* node;
    struct dtn_iter it;
    
    my_load_dtb();

    ctx = dt_main_context();

    dt_begin_find_byname(&it, ctx->root, "pci");
    node = (struct dtn*)it.matched;

    testcase("root", {
        expect_notnull(node);
        expect_int(node->base.addr_c, 3);
        expect_int(node->base.sz_c, 2);
        expect_int(node->base.intr_c, 1);
    });

    map_and_mask_test(node);
}