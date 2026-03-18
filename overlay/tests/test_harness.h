#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _tests_run = 0;
static int _tests_passed = 0;
static int _tests_failed = 0;

#define ASSERT(cond, msg) do { \
    _tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (line %d): %s\n", __func__, __LINE__, msg); \
        _tests_failed++; \
    } else { \
        _tests_passed++; \
    } \
} while (0)

#define ASSERT_EQ(a, b, msg) ASSERT((a) == (b), msg)
#define ASSERT_NEQ(a, b, msg) ASSERT((a) != (b), msg)
#define ASSERT_STR_EQ(a, b, msg) ASSERT(strcmp((a), (b)) == 0, msg)

#define RUN_TEST(fn) do { \
    printf("  %s...\n", #fn); \
    fn(); \
} while (0)

#define TEST_REPORT() do { \
    printf("\n%d tests, %d passed, %d failed\n", _tests_run, _tests_passed, _tests_failed); \
    return _tests_failed > 0 ? 1 : 0; \
} while (0)

#endif
