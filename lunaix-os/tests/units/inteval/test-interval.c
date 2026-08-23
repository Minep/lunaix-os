#include "lunaix/spike.h"
#include <lunaix/ds/interval.h>
#include <testing/basic.h>
#include <lunaix/mm/valloc.h>


static inline struct sub_interval*
__insert_interval(struct interval* itree, unsigned long start, unsigned long length, 
                  bool fixed, int align)
{
    struct sub_interval* subi;
    bool ok;

    subi = valloc(sizeof(struct sub_interval));
    subi->start = start;
    subi->end = start + length - 1;
    
    ok = interval_insert_node(itree, subi, fixed, align);
    if (!ok) {
        vfree(subi);
        return NULL;
    }

    printf("insert: (%c) 0x%lx -> 0x%lx\n", fixed ? 'f': 'a', start, subi->start);

    return subi;
}

#define PUT(IT, start, len)     \
    __insert_interval(&IT, start, len, false, 0)

#define PUT_FIXED(IT, start, len)     \
    __insert_interval(&IT, start, len, true, 0)

#define PUT_ALIGN(IT, start, len, al)     \
    __insert_interval(&IT, start, len, false, al)

#define PUT_FIXED_ALIGN(IT, start, len, al)     \
    __insert_interval(&IT, start, len, true, al)

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
test_range_query_full_cover_should_result_one()
{
    struct interval itree = {};
    struct interval_iter iiter;
    struct sub_interval* si;

    interval_init(&itree, 0, 1000);

    expect_notnull(PUT(itree, 0, 100));
    expect_notnull(PUT(itree, 200, 20));
    expect_notnull(PUT(itree, 455, 1));

    interval_search_begin(&itree, &iiter, 100, 300);

    si = interval_search_next(&iiter);
    expect_notnull(si);
    expect_ulong(si->start, 200);
    
    expect_null(interval_search_next(&iiter));

    interval_search_end(&iiter);
    
    __tear_down(&itree.root->rb);
}

static void
test_range_query_partial_cover_should_result_one()
{
    struct interval itree = {};
    struct interval_iter iiter;
    struct sub_interval* si;

    interval_init(&itree, 0, 1000);

    expect_notnull(PUT(itree, 0, 100));
    expect_notnull(PUT(itree, 200, 20));
    expect_notnull(PUT(itree, 455, 1));

    interval_search_begin(&itree, &iiter, 150, 201);

    si = interval_search_next(&iiter);
    expect_notnull(si);
    expect_ulong(si->start, 200);
    
    expect_null(interval_search_next(&iiter));

    interval_search_end(&iiter);
    
    __tear_down(&itree.root->rb);
}

static void
test_range_query_no_cover_should_result_none()
{
    struct interval itree = {};
    struct interval_iter iiter;
    struct sub_interval* si;

    interval_init(&itree, 0, 1000);

    expect_notnull(PUT(itree, 10, 100));
    expect_notnull(PUT(itree, 200, 20));
    expect_notnull(PUT(itree, 455, 1));

    interval_search_begin(&itree, &iiter, 500, 500);
    
    expect_null(interval_search_next(&iiter));

    interval_search_end(&iiter);

    __tear_down(&itree.root->rb);
}

static void
test_range_query_empty_should_result_none()
{
    struct interval itree = {};
    struct interval_iter iiter;
    struct sub_interval* si;

    interval_init(&itree, 0, 1000);

    interval_search_begin(&itree, &iiter, 500, 500);
    
    expect_null(interval_search_next(&iiter));

    interval_search_end(&iiter);
}

static void
test_range_query_malformed_range_should_result_none()
{
    struct interval itree = {};
    struct interval_iter iiter;
    struct sub_interval* si;

    interval_init(&itree, 0, 1000);
    expect_notnull(PUT(itree, 10, 100));
    expect_notnull(PUT(itree, 200, 20));
    expect_notnull(PUT(itree, 455, 1));

    interval_search_begin(&itree, &iiter, 100, 10);
    
    expect_null(interval_search_next(&iiter));

    interval_search_end(&iiter);
    
    __tear_down(&itree.root->rb);
}

static void
test_range_query_end_point_inclusive_should_result_none()
{
    struct interval itree = {};
    struct interval_iter iiter;
    struct sub_interval* si;

    interval_init(&itree, 0, 1000);
    expect_notnull(PUT(itree, 10, 100));
    expect_notnull(PUT(itree, 200, 20));
    expect_notnull(PUT(itree, 455, 1));

    interval_search_begin(&itree, &iiter, 0, 10);
    
    expect_null(interval_search_next(&iiter));

    interval_search_end(&iiter);
    
    __tear_down(&itree.root->rb);
}

static void
test_range_query_end_point_inclusive_should_result_one()
{
    struct interval itree = {};
    struct interval_iter iiter;
    struct sub_interval* si;

    interval_init(&itree, 0, 1000);
    expect_notnull(PUT(itree, 10, 100));
    expect_notnull(PUT(itree, 200, 20));
    expect_notnull(PUT(itree, 455, 1));

    interval_search_begin(&itree, &iiter, 0, 11);
    
    expect_notnull(interval_search_next(&iiter));

    interval_search_end(&iiter);
    
    __tear_down(&itree.root->rb);
}

static void
test_range_query_should_result_all()
{
    struct interval itree = {};
    struct interval_iter iiter;
    struct sub_interval* si;

    interval_init(&itree, 0, 1000);

    expect_notnull(PUT(itree, 0, 100));
    expect_notnull(PUT(itree, 455, 1));
    expect_notnull(PUT(itree, 200, 20));

    interval_search_begin(&itree, &iiter, 0, 1000);

    si = interval_search_next(&iiter);
    expect_notnull(si);
    expect_ulong(si->start, 0);
    
    si = interval_search_next(&iiter);
    expect_notnull(si);
    expect_ulong(si->start, 200);

    si = interval_search_next(&iiter);
    expect_notnull(si);
    expect_ulong(si->start, 455);
    
    expect_null(interval_search_next(&iiter));

    interval_search_end(&iiter);
    
    __tear_down(&itree.root->rb);
}

static void
test_range_query_with_on_fly_delete_at_root_should_work()
{
    struct interval itree = {};
    struct interval_iter iiter;
    struct sub_interval* si;

    interval_init(&itree, 0, 1000);

    expect_notnull(PUT(itree, 0, 100));
    expect_notnull(PUT(itree, 200, 20));        // our root
    expect_notnull(PUT(itree, 455, 1));

    interval_search_begin(&itree, &iiter, 0, 1000);

    si = interval_search_next(&iiter);
    expect_notnull(si);
    expect_ulong(si->start, 0);
    
    si = interval_search_next(&iiter);
    expect_notnull(si);
    expect_ulong(si->start, 200);

    // delete the tree root as well as our iterator pivot,
    // the iterator should go on just fine
    interval_delete(&itree, si);
    vfree(si);

    si = interval_search_next(&iiter);
    expect_notnull(si);
    expect_ulong(si->start, 455);
    
    expect_null(interval_search_next(&iiter));

    interval_search_end(&iiter);
    
    __tear_down(&itree.root->rb);
}

static void
test_insert_aligned_should_adjusted_to_boundary()
{
    struct interval itree = {};

    interval_init(&itree, 0x0, 0x1000000);

    expect_notnull(PUT(itree, 0x234, 20));

    // this will goto 0x2000
    expect_notnull(PUT_ALIGN(itree, 0x1234, 20, 12));

    // because 0x2000 already taken, bailed to next slot
    expect_notnull(PUT_ALIGN(itree, 0x1234, 20, 12));

    expect_notnull(interval_query(&itree, 0x2004));
    expect_notnull(interval_query(&itree, 0x3004));

    __tear_down(&itree.root->rb);
}

static void
test_insert_fixed_aligned_should_failed_for_unaligned_location()
{
    struct interval itree = {};

    interval_init(&itree, 0x0, 0x1000000);
    expect_null(PUT_FIXED_ALIGN(itree, 0x1234, 20, 12));
}

static void
test_insert_should_search_forward_when_tail_is_full()
{
    struct interval itree = {};

    interval_init(&itree, 0, 200);

    expect_notnull(PUT(itree, 100, 50)); // =100
    expect_notnull(PUT(itree, 150, 50)); // =150
    expect_notnull(PUT(itree, 150, 50)); // bail to 50

    expect_notnull(interval_query(&itree, 50));

    __tear_down(&itree.root->rb);
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

    testcase_fn(test_range_query_full_cover_should_result_one);
    testcase_fn(test_range_query_partial_cover_should_result_one);
    testcase_fn(test_range_query_no_cover_should_result_none);
    testcase_fn(test_range_query_empty_should_result_none);
    testcase_fn(test_range_query_malformed_range_should_result_none);
    testcase_fn(test_range_query_end_point_inclusive_should_result_one);
    testcase_fn(test_range_query_end_point_inclusive_should_result_none);
    testcase_fn(test_range_query_should_result_all);
    testcase_fn(test_range_query_with_on_fly_delete_at_root_should_work);

    testcase_fn(test_insert_aligned_should_adjusted_to_boundary);
    testcase_fn(test_insert_fixed_aligned_should_failed_for_unaligned_location);
    testcase_fn(test_insert_should_search_forward_when_tail_is_full);
}
