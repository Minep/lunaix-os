#ifndef __COMMON_TEST_BASIC_H
#define __COMMON_TEST_BASIC_H

#include <stdio.h>
#include "strutils.h"

struct test_context
{
    const char* name;

    struct {
        union {
            struct {
                int passed;
                int failed;
            };
            int countings[2];
        };

        union {
            struct {
                int total_passed;
                int total_failed;
            };
            int total_countings[2];
        };
    } stats;
};

#define active_context      \
    ({ extern struct test_context* __test_ctx; __test_ctx; })

#define _expect(expr, act, exp, type_fmt)                                   \
    do {                                                                    \
        int failed = !(expr);                                               \
        printf("  @%s:%d ....... ", __FILE__, __LINE__);                    \
        if (failed)                                                         \
            printf("failed. (expect: " type_fmt ", got: " type_fmt ")\n",   \
                    exp, act);                                              \
        else                                                                \
            printf("passed.\n");                                            \
        active_context->stats.countings[failed]++;                          \
    } while(0)

#define new_aspect(expr, exp, act, typefmt)    expr, exp, act, typefmt

#define expect(aspect)  _expect(aspect)

#define expect_int(a, b)  \
    _expect(a == b, a, b, "%d")

#define expect_uint(a, b) \
    _expect(a == b, a, b, "%u")

#define expect_long(a, b) \
    _expect(a == b, a, b, "%ld")

#define expect_ulong(a, b) \
    _expect(a == b, a, b, "%lu")

#define expect_str(str_a, str_b)  \
    _expect(!strcmp(str_a, str_b), str_a, str_b, "'%s'")

#define expect_notnull(ptr)  \
    _expect(ptr != NULL, "not null", "null", "<%s>")

#define expect_true(val)  \
    _expect(val, "false", "true", "<%s>")

#define expect_false(val)  \
    _expect(!val, "true", "false", "<%s>")

void
begin_testcase(const char* name);

void
end_testcase();

void 
run_test(int argc, const char* argv[]);

#endif /* __COMMON_TEST_BASIC_H */
