// test_boolring.c: the Boolean ring (boolring.h). The four step polynomials
// cut to x² = x have the dissection's sizes and agree with the general Poly
// and the compiled stepper on all 64 assignments; the ring laws hold on
// random small polynomials, and evaluation is a ring map; the printing is
// canonical; an overflow stops loudly.

#include "test_stops.h"

#include "../src/vm.c"
#include "../src/stepper.c"
#include "../src/boolring.c"
#include "../src/poly.c"
#include "../src/ring_int.c"

// The six step variables, bit s for stepper.h's slot s.
static BVars step_vars(void) {
    BVars v = {{"hb", "lb", "halted", "accepted", "op_hb", "op_lb"}, NUM_SLOTS};
    return v;
}

static int all_unit_coeffs(const BPoly *p) {
    for (size_t i = 0; i < p->len; i++) {
        if (p->terms[i].coeff != 1 && p->terms[i].coeff != -1) return 0;
    }
    return 1;
}

TEST(the_step_polynomials_cut_to_x_squared_is_x_have_the_dissections_sizes) {
    const StepPolys *s = step_polynomials();
    BVars v = step_vars();
    const Poly *polys[4] = {&s->hb, &s->lb, &s->halted, &s->accepted};
    const size_t want_terms[4] = {7, 13, 17, 33};
    const int want_degree[4] = {5, 5, 5, 6};
    for (int k = 0; k < 4; k++) {
        BPoly b = bp_from_poly(polys[k], &v);
        EXPECT_EQ(b.len, want_terms[k]);
        EXPECT_EQ(bp_degree(&b), want_degree[k]);
        EXPECT(all_unit_coeffs(&b));
        bp_free(&b);
    }
}

TEST(the_cut_step_polynomials_agree_with_poly_and_the_compiled_stepper_on_all_64) {
    const StepPolys *s = step_polynomials();
    BVars v = step_vars();
    const Poly *polys[4] = {&s->hb, &s->lb, &s->halted, &s->accepted};
    for (int k = 0; k < 4; k++) {
        BPoly b = bp_from_poly(polys[k], &v);
        CompiledPoly c = compile_step_poly(polys[k]);
        for (uint32_t ones = 0; ones < 64; ones++) {
            int64_t vals[NUM_SLOTS];
            Binding bind[NUM_SLOTS];
            for (int slot = 0; slot < NUM_SLOTS; slot++) {
                vals[slot] = (ones >> slot) & 1;
                bind[slot] = (Binding){v.names[slot], vals[slot]};
            }
            int64_t got = bp_eval(&b, ones);
            EXPECT_EQ(got, compiled_eval(&c, vals));
            EXPECT_EQ(got, poly_eval(polys[k], bind, NUM_SLOTS));
        }
        compiled_free(&c);
        bp_free(&b);
    }
}

TEST(the_cut_halted_polynomial_prints_readably) {
    BVars v = step_vars();
    const StepPolys *s = step_polynomials();
    BPoly b = bp_from_poly(&s->halted, &v);
    char *got = bp_str(&b, &v);
    // halted' = halted OR (AX = 0 AND decr AND NOT halted): halted, plus
    // (1-hb)(1-lb) op_hb (1-op_lb) (1-halted), expanded.
    const char *want =
        "halted+op_hb+(-1)*hb*op_hb+(-1)*lb*op_hb+(-1)*halted*op_hb+(-1)*op_hb*op_lb"
        "+hb*lb*op_hb+hb*halted*op_hb+hb*op_hb*op_lb+lb*halted*op_hb+lb*op_hb*op_lb"
        "+halted*op_hb*op_lb+(-1)*hb*lb*halted*op_hb+(-1)*hb*lb*op_hb*op_lb"
        "+(-1)*hb*halted*op_hb*op_lb+(-1)*lb*halted*op_hb*op_lb+hb*lb*halted*op_hb*op_lb";
    if (strcmp(got, want) != 0) fprintf(stderr, "  got %s\n", got);
    EXPECT(strcmp(got, want) == 0);
    free(got);
    bp_free(&b);
}

TEST(small_polynomials_print_in_canonical_order) {
    BVars v = {{"x", "y", "z"}, 3};
    BPoly x = bp_var(0);
    BPoly y = bp_var(1);
    BPoly z = bp_var(2);
    BPoly one = bp_constant(1);
    BPoly zero = bp_zero();
    // (1 - x)(y + z) * 3 - 2 = 3y + 3z - 3xy - 3xz - 2
    BPoly nx = bp_sub(&one, &x);
    BPoly yz = bp_add(&y, &z);
    BPoly prod = bp_mul(&nx, &yz);
    BPoly three = bp_constant(3);
    BPoly p3 = bp_mul(&prod, &three);
    BPoly two = bp_constant(2);
    BPoly p = bp_sub(&p3, &two);
    char *s = bp_str(&p, &v);
    EXPECT(strcmp(s, "(-2)+3*y+3*z+(-3)*x*y+(-3)*x*z") == 0);
    free(s);
    s = bp_str(&zero, &v);
    EXPECT(strcmp(s, "0") == 0);
    free(s);
    BPoly xx = bp_mul(&x, &x);
    EXPECT(bp_equal(&xx, &x));
    BPoly *all[] = {&x, &y, &z, &one, &zero, &nx, &yz, &prod, &three, &p3, &two, &p, &xx};
    for (size_t i = 0; i < sizeof all / sizeof all[0]; i++) bp_free(all[i]);
}

// ── Ring laws on random small polynomials ────────────────────────────────

static uint64_t rng = 88172645463325252ull;

static uint32_t next_rand(void) {
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 16);
}

// Up to 6 terms over 5 variables, coefficients -3..3.
static BPoly random_poly(void) {
    BPoly p = bp_zero();
    int n = (int)(next_rand() % 7);
    for (int i = 0; i < n; i++) {
        BPoly t = bp_constant((int64_t)(next_rand() % 7) - 3);
        uint32_t mask = next_rand() % 32;
        for (int bit = 0; bit < 5; bit++) {
            if (mask & (1u << bit)) {
                BPoly x = bp_var(bit);
                BPoly tx = bp_mul(&t, &x);
                bp_free(&t);
                bp_free(&x);
                t = tx;
            }
        }
        BPoly sum = bp_add(&p, &t);
        bp_free(&p);
        bp_free(&t);
        p = sum;
    }
    return p;
}

// 1 when p is canonical: masks strictly ascending, no zero coefficient.
static int is_canonical(const BPoly *p) {
    for (size_t i = 0; i < p->len; i++) {
        if (p->terms[i].coeff == 0) return 0;
        if (i > 0 && p->terms[i - 1].mask >= p->terms[i].mask) return 0;
    }
    return 1;
}

TEST(the_ring_laws_hold_and_eval_is_a_ring_map) {
    for (int round = 0; round < 300; round++) {
        BPoly a = random_poly();
        BPoly b = random_poly();
        BPoly c = random_poly();
        BPoly ab = bp_mul(&a, &b);
        BPoly ba = bp_mul(&b, &a);
        BPoly a_plus_b = bp_add(&a, &b);
        BPoly b_plus_a = bp_add(&b, &a);
        BPoly ab_c = bp_mul(&ab, &c);
        BPoly bc = bp_mul(&b, &c);
        BPoly a_bc = bp_mul(&a, &bc);
        BPoly b_plus_c = bp_add(&b, &c);
        BPoly a_times_b_plus_c = bp_mul(&a, &b_plus_c);
        BPoly ac = bp_mul(&a, &c);
        BPoly ab_plus_ac = bp_add(&ab, &ac);
        BPoly a_minus_a = bp_sub(&a, &a);
        BPoly neg_a = bp_neg(&a);
        BPoly a_plus_neg_a = bp_add(&a, &neg_a);
        BPoly aa = bp_mul(&a, &a);
        EXPECT(bp_equal(&ab, &ba));
        EXPECT(bp_equal(&a_plus_b, &b_plus_a));
        EXPECT(bp_equal(&ab_c, &a_bc));
        EXPECT(bp_equal(&a_times_b_plus_c, &ab_plus_ac));
        EXPECT_EQ(a_minus_a.len, 0);
        EXPECT_EQ(a_plus_neg_a.len, 0);
        EXPECT(is_canonical(&ab) && is_canonical(&a_plus_b) && is_canonical(&ab_c) && is_canonical(&aa));
        for (uint32_t ones = 0; ones < 32; ones++) {
            int64_t va = bp_eval(&a, ones);
            int64_t vb = bp_eval(&b, ones);
            EXPECT_EQ(bp_eval(&ab, ones), va * vb);
            EXPECT_EQ(bp_eval(&a_plus_b, ones), va + vb);
            EXPECT_EQ(bp_eval(&neg_a, ones), -va);
            EXPECT_EQ(bp_eval(&aa, ones), va * va);
        }
        BPoly *all[] = {&a, &b, &c, &ab, &ba, &a_plus_b, &b_plus_a, &ab_c, &bc, &a_bc, &b_plus_c,
                        &a_times_b_plus_c, &ac, &ab_plus_ac, &a_minus_a, &neg_a, &a_plus_neg_a, &aa};
        for (size_t i = 0; i < sizeof all / sizeof all[0]; i++) bp_free(all[i]);
    }
}

TEST(substitute_is_composition) {
    // p = x*y - z over x, y, z; x := u, y := 1 - u, z := u*w over u, w.
    BPoly x = bp_var(0);
    BPoly y = bp_var(1);
    BPoly z = bp_var(2);
    BPoly xy = bp_mul(&x, &y);
    BPoly p = bp_sub(&xy, &z);
    BPoly u = bp_var(0);
    BPoly w = bp_var(1);
    BPoly one = bp_constant(1);
    BPoly vals[3] = {bp_copy(&u), bp_sub(&one, &u), bp_mul(&u, &w)};
    BPoly got = bp_substitute(&p, vals, 3);
    // u(1 - u) is 0 with u² = u, so the result is -u*w.
    BVars names = {{"u", "w"}, 2};
    char *s = bp_str(&got, &names);
    EXPECT(strcmp(s, "(-1)*u*w") == 0);
    free(s);
    BPoly *all[] = {&x, &y, &z, &xy, &p, &u, &w, &one, &vals[0], &vals[1], &vals[2], &got};
    for (size_t i = 0; i < sizeof all / sizeof all[0]; i++) bp_free(all[i]);
}

static void mul_overflows(void) {
    BPoly a = bp_constant(INT64_MAX / 2 + 1);
    BPoly two = bp_constant(2);
    bp_mul(&a, &two);
}

static void add_overflows(void) {
    BPoly a = bp_constant(INT64_MAX);
    BPoly one = bp_constant(1);
    bp_add(&a, &one);
}

static void like_terms_overflow(void) {
    // (M x + M) * (x + 1) = 3M x + M, x² = x: the like terms' sum overflows.
    BPoly x = bp_var(0);
    BPoly m = bp_constant(INT64_MAX / 2);
    BPoly mx = bp_mul(&m, &x);
    BPoly p = bp_add(&mx, &m);
    BPoly one = bp_constant(1);
    BPoly q = bp_add(&x, &one);
    bp_mul(&p, &q);
}

static void neg_overflows(void) {
    BPoly a = bp_constant(INT64_MIN);
    bp_neg(&a);
}

static void unknown_variable(void) {
    Poly q = poly_var("q");
    BVars v = {{"x"}, 1};
    bp_from_poly(&q, &v);
}

TEST(an_overflow_or_an_unknown_variable_stops_loudly) {
    EXPECT_STOPS(mul_overflows, "int64 overflow in mul(4611686018427387904, 2)");
    EXPECT_STOPS(add_overflows, "int64 overflow in add(9223372036854775807, 1)");
    EXPECT_STOPS(like_terms_overflow, "int64 overflow in add");
    EXPECT_STOPS(neg_overflows, "int64 overflow in mul(-1, -9223372036854775808)");
    EXPECT_STOPS(unknown_variable, "boolring: q is not one of the variables");
}

TEST_MAIN(TEST_CASE(the_step_polynomials_cut_to_x_squared_is_x_have_the_dissections_sizes),
          TEST_CASE(the_cut_step_polynomials_agree_with_poly_and_the_compiled_stepper_on_all_64),
          TEST_CASE(the_cut_halted_polynomial_prints_readably),
          TEST_CASE(small_polynomials_print_in_canonical_order),
          TEST_CASE(the_ring_laws_hold_and_eval_is_a_ring_map),
          TEST_CASE(substitute_is_composition),
          TEST_CASE(an_overflow_or_an_unknown_variable_stops_loudly))
