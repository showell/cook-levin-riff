// test_ring_int.c: the checked int64 ring, at its edges, and its loud stops.

#include "test_stops.h"

#include "../src/ring_int.c"

TEST(add_mul_negate_power_agree_with_python_inside_int64) {
    EXPECT_EQ(ring_add(2, 3), 5);
    EXPECT_EQ(ring_add(-7, 3), -4);
    EXPECT_EQ(ring_add(INT64_MAX, -1), INT64_MAX - 1);
    EXPECT_EQ(ring_add(INT64_MIN, INT64_MAX), -1);
    EXPECT_EQ(ring_mul(-6, 7), -42);
    EXPECT_EQ(ring_mul(INT64_MIN, 1), INT64_MIN);
    EXPECT_EQ(ring_mul(INT64_MAX, -1), -INT64_MAX);
    EXPECT_EQ(ring_mul(0, INT64_MIN), 0);
    EXPECT_EQ(ring_negate(-5), 5);
    EXPECT_EQ(ring_negate(INT64_MAX), -INT64_MAX);
    EXPECT_EQ(ring_power(0, 0), 1);
    EXPECT_EQ(ring_power(5, 0), 1);
    EXPECT_EQ(ring_power(0, 5), 0);
    EXPECT_EQ(ring_power(1, 1000000000), 1);
    EXPECT_EQ(ring_power(-1, 1000000001), -1);
    EXPECT_EQ(ring_power(-2, 63), INT64_MIN);
    EXPECT_EQ(ring_power(3, 39), 4052555153018976267LL);
    EXPECT_EQ(ring_power(-3, 3), -27);
}

static void add_past_max(void) { ring_add(INT64_MAX, 1); }
static void add_past_min(void) { ring_add(INT64_MIN, -1); }
static void mul_past_max(void) { ring_mul(INT64_MAX / 2 + 1, 2); }
static void mul_of_min_by_minus_one(void) { ring_mul(INT64_MIN, -1); }
static void mul_past_min(void) { ring_mul(-3037000500LL, 3037000500LL); }
static void negate_min(void) { ring_negate(INT64_MIN); }
static void power_past_max(void) { ring_power(2, 63); }
static void power_of_3_past_max(void) { ring_power(3, 40); }
static void negative_power(void) { ring_power(2, -1); }

TEST(an_overflow_stops_naming_the_operation_and_its_operands) {
    EXPECT_STOPS(add_past_max, "int64 overflow in add(9223372036854775807, 1)");
    EXPECT_STOPS(add_past_min, "int64 overflow in add(-9223372036854775808, -1)");
    EXPECT_STOPS(mul_past_max, "int64 overflow in mul(4611686018427387904, 2)");
    EXPECT_STOPS(mul_of_min_by_minus_one, "int64 overflow in mul(-9223372036854775808, -1)");
    EXPECT_STOPS(mul_past_min, "int64 overflow in mul(-3037000500, 3037000500)");
    EXPECT_STOPS(negate_min, "int64 overflow in negate(-9223372036854775808)");
    EXPECT_STOPS(power_past_max, "int64 overflow in power(2, 63)");
    EXPECT_STOPS(power_of_3_past_max, "int64 overflow in power(3, 40)");
    EXPECT_STOPS(negative_power, "negative exponent in power(2, -1)");
}

TEST_MAIN(TEST_CASE(add_mul_negate_power_agree_with_python_inside_int64),
          TEST_CASE(an_overflow_stops_naming_the_operation_and_its_operands))
