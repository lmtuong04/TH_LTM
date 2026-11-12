/* Bộ test tối giản: CHECK không dừng chương trình, cuối file gọi TEST_DONE(). */
#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>

static int test_failures = 0;
static int test_checks = 0;

#define CHECK(cond) do { \
        test_checks++; \
        if (!(cond)) { \
            test_failures++; \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)

#define RUN(fn) do { printf("  %s\n", #fn); fn(); } while (0)

#define TEST_DONE() ( \
        printf("  %d/%d check PASS\n", test_checks - test_failures, test_checks), \
        test_failures == 0 ? 0 : 1)

#endif
