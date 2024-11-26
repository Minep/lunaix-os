#include "common.h"
#include <testing/basic.h>

extern void *calloc(size_t, size_t);
extern void  free(void *);

#define be(v) ((((v) & 0x000000ff) << 24)  |\
               (((v) & 0x00ff0000) >> 8 )  |\
               (((v) & 0x0000ff00) << 8 )  |\
               (((v) & 0xff000000) >> 24))

static void
map_and_mask_test(struct dtn* node)
{
    struct dtn_intr* intrn;
    struct dtspec_map *map;
    struct dtspec_key *mask, *key, test_key;

    intrn = &node->intr;
    map = intrn->map;
    mask = &map->mask;

    test_key.val = calloc(4, sizeof(int));
    test_key.size = 4;
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

    testcase("key-copy", {
        struct dtspec_key tmp_key;        
        dtspec_cpykey(&tmp_key, &test_key);

        expect_int(tmp_key.bval->raw[0], 0xf200);
        expect_int(tmp_key.bval->raw[1], 0x22);
        expect_int(tmp_key.bval->raw[2], 0);
        expect_int(tmp_key.bval->raw[3], 0x16);
        expect_int(tmp_key.size, test_key.size);

        dtspec_freekey(&tmp_key);
        expect_true(dtspec_nullkey(&tmp_key));
    });

    testcase("mask-ops", {
        struct dtspec_key tmp_key;
        dtspec_cpykey(&tmp_key, &test_key);

        dtspec_applymask(map, &tmp_key);

        expect_int(tmp_key.bval->raw[0], 0xf000);
        expect_int(tmp_key.bval->raw[1], 0);
        expect_int(tmp_key.bval->raw[2], 0);
        expect_int(tmp_key.bval->raw[3], 6);

        dtspec_freekey(&tmp_key);
    });

    testcase("map-get", {
        struct dtspec_mapent* ent;

        test_key.bval->raw[0] = 0x8820;
        test_key.bval->raw[1] = 0x22;
        test_key.bval->raw[2] = 0x0;
        test_key.bval->raw[3] = 0x14;

        ent = dtspec_lookup(map, &test_key);
        expect_notnull(ent);

        expect_int(ent->child_spec.bval->raw[0], 0x8800);
        expect_int(ent->child_spec.bval->raw[1], 0);
        expect_int(ent->child_spec.bval->raw[2], 0);
        expect_int(ent->child_spec.bval->raw[3], 4);

        expect_notnull(ent->parent);
        expect_str(morpher_name(dt_mobj(ent->parent)), "pic@1");

        expect_int(ent->parent_spec.bval->raw[0], 1);
        expect_int(ent->parent_spec.bval->raw[1], 1);
    });

    free(test_key.val);
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