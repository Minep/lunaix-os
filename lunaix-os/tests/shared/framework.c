#include <testing/basic.h>
#include <stdlib.h>

struct test_context* __test_ctx;

void 
main(int argc, const char* argv[])
{
    __test_ctx = calloc(sizeof(*__test_ctx), 1);

    run_test(argc, argv);

    printf(
        "All test done: %d passed, %d failed\n\n",
        __test_ctx->stats.total_passed,
        __test_ctx->stats.total_failed
    );

    exit(__test_ctx->stats.total_failed > 0);
}

void
begin_testcase(const char* name)
{
    if (__test_ctx->name) {
        printf("previous test case: '%s' is still actuive\n", name);
        exit(1);
    }

    __test_ctx->name = name;
    __test_ctx->stats.countings[0] = 0;
    __test_ctx->stats.countings[1] = 0;

    printf("%s\n", name);
}

void
end_testcase()
{
    printf("..... passed: %d, failed: %d\n\n", 
            __test_ctx->stats.passed, __test_ctx->stats.failed);

    __test_ctx->stats.total_passed += __test_ctx->stats.passed;
    __test_ctx->stats.total_failed += __test_ctx->stats.failed;
    __test_ctx->name = NULL;

}