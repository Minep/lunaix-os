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
                int skipped;
            };
            int countings[3];
        };

        union {
            struct {
                int total_passed;
                int total_failed;
                int total_skipped;
            };
            int total_countings[3];
        };
    } stats;
};

#define fmt_passed  "[\x1b[32;49mPASS\x1b[0m]"
#define fmt_failed  "[\x1b[31;49mFAIL\x1b[0m]"
#define fmt_skiped  "[\x1b[33;49mSKIP\x1b[0m]"

#define active_context      \
    ({ extern struct test_context* __test_ctx; __test_ctx; })

#define _expect(expr, act, exp, type_fmt)                                   \
    do {                                                                    \
        if(should_skip_test()) {                                            \
            printf("  @%s:%03d ....... ", __FILE__, __LINE__);              \
            printf(fmt_skiped "\n");                                        \
            active_context->stats.skipped++;                                \
            break;                                                          \
        }                                                                   \
                                                                            \
        int failed = !(expr);                                               \
        if (failed) {                                                       \
            printf("  @%s:%03d ....... ", __FILE__, __LINE__);              \
            printf(fmt_failed " (expect: " type_fmt ", got: " type_fmt ")\n",\
                    exp, act);                                              \
        }                                                                   \
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
    _expect(ptr != NULL, "null", "not null", "<%s>")

#define expect_null(ptr)  \
    _expect(ptr == NULL, "not null", "null", "<%s>")

#define expect_true(val)  \
    _expect(val, "false", "true", "<%s>")

#define expect_false(val)  \
    _expect(!val, "true", "false", "<%s>")

#define testcase(name, body)    \
    do { begin_testcase(name); body; end_testcase(); } while(0)

void
begin_testcase(const char* name);

void
end_testcase();

void 
run_test(int argc, const char* argv[]);

static inline int
should_skip_test()
{
    return active_context->stats.failed;
}

#endif /* __COMMON_TEST_BASIC_H */
