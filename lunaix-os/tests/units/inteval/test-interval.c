#include "lunaix/spike.h"
#include <lunaix/ds/interval.h>
#include <testing/basic.h>
#include <lunaix/mm/valloc.h>


static inline struct sub_interval*
__insert_interval(struct interval* itree, unsigned long start, unsigned long length, bool fixed)
{
    struct sub_interval* subi;
    bool ok;

    subi = valloc(sizeof(struct sub_interval));
    subi->start = start;
    subi->end = start + length - 1;
    
    if (fixed) {
        ok = interval_insert(itree, subi);
    } else {
        ok = interval_alloc_insert(itree, subi);
    }

    if (!ok) {
        vfree(subi);
        return NULL;
    }

    printf("insert: (%c) 0x%lx -> 0x%lx\n", fixed ? 'f': 'a', start, subi->start);

    return subi;
}

#define PUT(IT, start, len)     \
    __insert_interval(&IT, start, len, false)

#define PUT_FIXED(IT, start, len)     \
    __insert_interval(&IT, start, len, true)

#define node_of(rbnode) \
    container_of(rbnode, struct sub_interval, rb)

static void
__tear_down(struct rbnode* root)
{
    if (!root)
        return;

    __tear_down(root->right);
    __tear_down(root->left);
    vfree(node_of(root));
}

static void
test_should_fail_uninitialized()
{
    struct interval itree = {};

    expect_null(PUT(itree, 0x12344, 0x100));
}

static void
test_should_fail_insert_out_of_bound()
{
    struct interval itree = {};

    interval_init(&itree, 0x0, 0x1000);

    expect_notnull(PUT(itree, 0x10, 0x100));
    expect_notnull(PUT(itree, 0xffe, 0x1));
    expect_null(PUT(itree, 0x2000, 0x100));
    expect_null(PUT(itree, 0x1000, 0x1));

    __tear_down(&itree.root->rb);
}

static void
test_query_from_empty_interval()
{
    struct interval itree = {};

    interval_init(&itree, 0x0, 0x1000000);
    expect_null(interval_query(&itree, 0x12345));
}

static void
test_query_one_interval_ok()
{
    struct interval itree = {};

    interval_init(&itree, 0x0, 0x1000000);
    expect_notnull(PUT(itree, 0x100, 0x200));

    expect_notnull(interval_query(&itree, 0x133));

    __tear_down(&itree.root->rb);
}

static void
test_query_interval_with_extremity()
{
    struct interval itree = {};

    interval_init(&itree, 0x0, 0x1000000);

    expect_notnull(PUT(itree, 0x100, 0x100));

    expect_null(interval_query(&itree, 0x0ff));
    expect_notnull(interval_query(&itree, 0x100));
    expect_notnull(interval_query(&itree, 0x1ff));
    expect_null(interval_query(&itree, 0x200));

    __tear_down(&itree.root->rb);
}

static void
test_query_from_two_sparse_intervals()
{

    struct interval itree = {};

    interval_init(&itree, 0x0, 0x1000000);

    expect_notnull(PUT(itree, 0x100, 0x200));
    expect_notnull(PUT(itree, 0x800000, 0x10000));

    expect_null(interval_query(&itree, 0x1000));
    expect_notnull(interval_query(&itree, 0x150));
    expect_notnull(interval_query(&itree, 0x801fff));

    __tear_down(&itree.root->rb);
}

static void
test_query_from_two_adjacent_intervals()
{

    struct interval itree = {};

    interval_init(&itree, 0x0, 0x1000000);

    expect_notnull(PUT(itree, 0x100, 0x200));
    expect_notnull(PUT(itree, 0x300, 0x200));

    expect_notnull(interval_query(&itree, 0x2ff));
    expect_notnull(interval_query(&itree, 0x300));
    expect_notnull(interval_query(&itree, 0x4ff));

    __tear_down(&itree.root->rb);
}

static void
test_should_success_non_fixed_insert_partial_overlapped_interval()
{
    struct interval itree = {};

    interval_init(&itree, 0x0, 0x1000000);

    expect_notnull(PUT(itree, 0x100, 0x200));

    expect_notnull(PUT(itree, 0x150, 0x200));
    expect_notnull(PUT(itree, 0x1ff, 0x200));
    expect_notnull(PUT(itree, 0x0, 0x101));

    __tear_down(&itree.root->rb);
}

static void
test_should_success_non_fixed_insert_full_overlapped_interval()
{
    struct interval itree = {};

    interval_init(&itree, 0x0, 0x1000000);

    expect_notnull(PUT(itree, 0x100, 0x200));
    expect_notnull(PUT(itree, 0x100, 0x200));
    expect_notnull(PUT(itree, 0x100, 0x200));
    expect_notnull(PUT(itree, 0x100, 0x200));

    __tear_down(&itree.root->rb);
}

static void
test_should_success_insert_at_fixed_location()
{

    struct interval itree = {};

    interval_init(&itree, 0x0, 0x1000000);

    expect_notnull(PUT_FIXED(itree, 0x100, 0x200));
    expect_notnull(PUT_FIXED(itree, 0x300, 0x200));
    expect_notnull(PUT_FIXED(itree, 0x5000, 0x200));

    __tear_down(&itree.root->rb);
}

static void
test_should_fail_insert_overlapped_at_fixed_location()
{
    struct interval itree = {};

    interval_init(&itree, 0x0, 0x1000000);

    expect_notnull(PUT_FIXED(itree, 0x100, 0x200));
    expect_null(PUT_FIXED(itree, 0x150, 0x200));

    __tear_down(&itree.root->rb);
}

static void
test_should_fail_when_no_room()
{
    struct interval itree = {};

    interval_init(&itree, 0, 100);

    expect_notnull(PUT(itree, 0, 50));
    expect_notnull(PUT(itree, 60, 40));
    expect_null(PUT(itree, 50, 11));

    __tear_down(&itree.root->rb);
}

static void
test_should_not_fail_when_tiled()
{
    struct interval itree = {};

    interval_init(&itree, 0, 100);

    expect_notnull(PUT(itree, 25, 25));
    expect_notnull(PUT(itree, 75, 25));
    expect_notnull(PUT(itree, 50, 25));
    expect_notnull(PUT(itree, 0, 25));

    __tear_down(&itree.root->rb);
}

static void
test_range_query_should_result_one()
{

}

static void
test_range_query_should_result_all()
{

}

#define testcase_fn(fn) \
    testcase(#fn, fn())

void 
run_test(int argc, const char* argv[])
{
    testcase_fn(test_should_fail_uninitialized);
    testcase_fn(test_should_fail_insert_out_of_bound);

    testcase_fn(test_query_from_empty_interval);
    testcase_fn(test_query_one_interval_ok);
    testcase_fn(test_query_interval_with_extremity);
    testcase_fn(test_query_from_two_adjacent_intervals);
    testcase_fn(test_query_from_two_sparse_intervals);

    testcase_fn(test_should_success_non_fixed_insert_partial_overlapped_interval);
    testcase_fn(test_should_success_non_fixed_insert_full_overlapped_interval);
    testcase_fn(test_should_success_insert_at_fixed_location);
    testcase_fn(test_should_fail_insert_overlapped_at_fixed_location);

    testcase_fn(test_should_fail_when_no_room);
    testcase_fn(test_should_not_fail_when_tiled);

}
