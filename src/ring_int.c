// ring_int.c: checked int64 arithmetic (ring_int.h). The checks compare
// against INT64_MAX and INT64_MIN rather than use gcc's
// __builtin_*_overflow, which tcc lacks.

#include "ring_int.h"

#include <stdio.h>
#include <stdlib.h>

static void ring_overflow(const char *op, int64_t a, int64_t b) {
    fprintf(stderr, "ring_int: int64 overflow in %s(%lld, %lld)\n", op, (long long)a, (long long)b);
    abort();
}

// a + b into *out; 0 when it would overflow.
static int add_fits(int64_t a, int64_t b, int64_t *out) {
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return 0;
    *out = a + b;
    return 1;
}

// a * b into *out; 0 when it would overflow.
static int mul_fits(int64_t a, int64_t b, int64_t *out) {
    if (a != 0 && b != 0) {
        if (a > 0) {
            if (b > 0 ? a > INT64_MAX / b : b < INT64_MIN / a) return 0;
        } else {
            if (b > 0 ? a < INT64_MIN / b : b < INT64_MAX / a) return 0;
        }
    }
    *out = a * b;
    return 1;
}

int64_t ring_add(int64_t a, int64_t b) {
    int64_t out;
    if (!add_fits(a, b, &out)) ring_overflow("add", a, b);
    return out;
}

int64_t ring_mul(int64_t a, int64_t b) {
    int64_t out;
    if (!mul_fits(a, b, &out)) ring_overflow("mul", a, b);
    return out;
}

int64_t ring_negate(int64_t n) {
    if (n == INT64_MIN) {
        fprintf(stderr, "ring_int: int64 overflow in negate(%lld)\n", (long long)n);
        abort();
    }
    return -n;
}

// By squaring, and a square is taken only while a higher bit of exp is
// left, so no square is bigger than the answer: an overflow on the way is
// an overflow of the answer.
int64_t ring_power(int64_t n, int64_t exp) {
    if (exp < 0) {
        fprintf(stderr, "ring_int: negative exponent in power(%lld, %lld)\n", (long long)n, (long long)exp);
        abort();
    }
    int64_t result = 1;
    int64_t base = n;
    int64_t e = exp;
    while (e > 0) {
        if (e & 1) {
            if (!mul_fits(result, base, &result)) ring_overflow("power", n, exp);
        }
        e >>= 1;
        if (e > 0 && !mul_fits(base, base, &base)) ring_overflow("power", n, exp);
    }
    return result;
}
