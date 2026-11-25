// test.h: the tests' harness, a small one modelled on Shale's
// c/foundation/test.h. A test is `TEST(name) { ... }`; a file ends with
// `TEST_MAIN(TEST_CASE(a), TEST_CASE(b), ...)`, which runs every case named,
// in order, and prints the exact count:
//
//     tests/test_vm.c: 3 tests passed
//
// A failure names its test and its line and ends that test (the rest still
// run), and the run exits 1. build.shale holds each file's printed count to
// its `TEST(`s, so a case left out of TEST_MAIN is a failure there (and a
// static function nothing calls, which gcc's -Wall -Werror refuses).

#ifndef COOK_LEVIN_TEST_H
#define COOK_LEVIN_TEST_H

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    void (*fn)(void);
} TestCase;

typedef struct {
    jmp_buf back;
    const char *current;
    size_t failed;
} TestState;

static TestState test_state;

#define TEST(name) static void test_##name(void)
#define TEST_CASE(name) {#name, test_##name}

static void test_fail(const char *file, int line, const char *what) {
    fprintf(stderr, "FAIL %s (%s:%d): %s\n", test_state.current, file, line, what);
    longjmp(test_state.back, 1);
}

#define EXPECT(cond)                                                    \
    do {                                                                \
        if (!(cond)) test_fail(__FILE__, __LINE__, #cond);              \
    } while (0)

#define EXPECT_EQ(a, b)                                                                   \
    do {                                                                                  \
        int64_t test_a_ = (int64_t)(a);                                                   \
        int64_t test_b_ = (int64_t)(b);                                                   \
        if (test_a_ != test_b_) {                                                         \
            fprintf(stderr, "  %s is %lld, %s is %lld\n", #a, (long long)test_a_, #b,     \
                    (long long)test_b_);                                                  \
            test_fail(__FILE__, __LINE__, #a " == " #b);                                  \
        }                                                                                 \
    } while (0)

// Runs one case, a failure counted. The setjmp is alone in this function,
// so no local of its caller lives across it (gcc's -Wclobbered at -O2).
static void test_run_case(const TestCase *c) {
    test_state.current = c->name;
    if (setjmp(test_state.back) == 0) {
        c->fn();
    } else {
        test_state.failed++;
    }
}

static int test_run(const TestCase *cases, size_t count, const char *file) {
    for (size_t i = 0; i < count; i++) test_run_case(&cases[i]);
    if (test_state.failed > 0) {
        fprintf(stderr, "%s: %zu of %zu tests failed\n", file, test_state.failed, count);
        return 1;
    }
    printf("%s: %zu tests passed\n", file, count);
    return 0;
}

#define TEST_MAIN(...)                                                              \
    int main(void) {                                                                \
        static const TestCase cases[] = {__VA_ARGS__};                              \
        return test_run(cases, sizeof cases / sizeof(TestCase), __FILE__);          \
    }

#endif
