// test_poly.c: the general Poly against poly.py. Every string here is what
// Python's `str` gives for the same expression (the docstring's asserts,
// and more computed by poly.py itself), held exactly; the golden files are
// poly.py's `str` of a product and a power.

#include "test_stops.h"

#include "../src/poly.c"
#include "../src/ring_int.c"

// The Polys a test makes, freed at its end by `release`.
static Poly kept[256];
static size_t kept_len;

static const Poly *K(Poly p) {
    if (kept_len == sizeof kept / sizeof kept[0]) {
        fprintf(stderr, "test_poly: too many Polys kept\n");
        abort();
    }
    kept[kept_len] = p;
    return &kept[kept_len++];
}

static void release(void) {
    for (size_t i = 0; i < kept_len; i++) poly_free(&kept[i]);
    kept_len = 0;
}

static const Poly *V(const char *name) { return K(poly_var(name)); }
static const Poly *C(int64_t c) { return K(poly_constant(c)); }
static const Poly *ADD(const Poly *p, const Poly *q) { return K(poly_add(p, q)); }
static const Poly *SUB(const Poly *p, const Poly *q) { return K(poly_sub(p, q)); }
static const Poly *MUL(const Poly *p, const Poly *q) { return K(poly_mul(p, q)); }
static const Poly *POW(const Poly *p, int64_t k) { return K(poly_pow(p, k)); }
static const Poly *NEG(const Poly *p) { return K(poly_neg(p)); }
// p + c, p - c, c * p and c - p, as Python spells them with an int.
static const Poly *ADDI(const Poly *p, int64_t c) { return K(poly_add_int(p, c)); }
static const Poly *SUBI(const Poly *p, int64_t c) { return SUB(p, C(c)); }
static const Poly *MULI(int64_t c, const Poly *p) { return K(poly_mul_int(p, c)); }
static const Poly *ISUB(int64_t c, const Poly *p) { return K(poly_int_sub(c, p)); }

static int str_is(const Poly *p, const char *want) {
    char *s = poly_str(p);
    int ok = strcmp(s, want) == 0;
    if (!ok) fprintf(stderr, "  got  %s\n  want %s\n", s, want);
    free(s);
    return ok;
}

#define EXPECT_STR(p, want) EXPECT(str_is(p, want))

// A golden file's text, its last newline dropped; the caller frees it.
static char *golden(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    char *s = NULL;
    size_t len = 0;
    size_t cap = 0;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (len + 2 > cap) {
            cap = cap ? cap * 2 : 4096;
            s = realloc(s, cap);
            if (s == NULL) abort();
        }
        s[len++] = (char)c;
    }
    fclose(f);
    if (s == NULL) return NULL;
    if (len > 0 && s[len - 1] == '\n') len--;
    s[len] = 0;
    return s;
}

TEST(the_docstrings_symbolic_manipulation) {
    const Poly *x = V("x");
    EXPECT_STR(MUL(ADDI(x, 1), SUBI(x, 1)), "(x**2)+(-1)");
    EXPECT_STR(MUL(ADDI(x, 2), ADDI(x, 3)), "(x**2)+5*x+6");
    EXPECT_STR(POW(ADDI(x, 2), 3), "(x**3)+6*(x**2)+12*x+8");
    EXPECT_STR(POW(ADDI(MULI(3, x), 1), 4), "81*(x**4)+108*(x**3)+54*(x**2)+12*x+1");
    release();
}

TEST(the_docstrings_multiple_variables) {
    const Poly *x = V("x");
    const Poly *y = V("y");
    const Poly *z = V("z");
    EXPECT_STR(SUB(ADD(ADD(x, y), z), y), "x+z");
    EXPECT_STR(MUL(ADD(x, y), ADD(z, y)), "x*y+x*z+(y**2)+y*z");
    release();
}

TEST(the_docstrings_substitute) {
    const Poly *x = V("x");
    const Poly *u = V("u");
    const Poly *got = K(poly_substitute(ADDI(MULI(2, x), 1), "x", ADDI(u, 3)));
    EXPECT(poly_equal(got, ADDI(MULI(2, u), 7)));
    EXPECT_STR(got, "2*u+7");
    release();
}

TEST(signs_print_in_parentheses_as_pythons_do) {
    const Poly *x = V("x");
    const Poly *y = V("y");
    EXPECT_STR(SUB(x, y), "x+(-1)*y");
    EXPECT_STR(NEG(x), "(-1)*x");
    EXPECT_STR(ISUB(1, x), "(-1)*x+1");
    EXPECT_STR(C(-3), "(-3)");
    EXPECT_STR(NEG(SUBI(x, 1)), "(-1)*x+1");
    EXPECT_STR(ISUB(5, MUL(x, y)), "(-1)*x*y+5");
    EXPECT_STR(POW(SUB(x, y), 2), "(x**2)+(-2)*x*y+(y**2)");
    release();
}

TEST(zero_one_and_the_shortcuts_print_as_pythons_do) {
    const Poly *x = V("x");
    EXPECT_STR(MULI(0, x), "0");
    EXPECT_STR(SUB(x, x), "0");
    EXPECT_STR(K(poly_zero()), "0");
    EXPECT_STR(C(0), "0");
    EXPECT_STR(POW(x, 0), "1");
    EXPECT_STR(K(poly_one()), "1");
    EXPECT_STR(MULI(3, x), "3*x");
    EXPECT_STR(ADDI(x, 0), "x");
    EXPECT_STR(MUL(C(1), x), "x");
    EXPECT(poly_is_zero(SUB(x, x)));
    EXPECT(poly_is_one(POW(x, 0)));
    release();
}

TEST(variables_sort_by_name_bytewise) {
    const Poly *a = V("a");
    const Poly *b = V("b");
    const Poly *ab = V("ab");
    const Poly *B = V("B");
    EXPECT_STR(MUL(MUL(MUL(b, a), ab), B), "B*a*ab*b");
    EXPECT_STR(ADDI(ADD(ADD(ADD(ab, a), b), B), 1), "B+a+ab+b+1");
    EXPECT_STR(MUL(ADD(a, B), SUB(ab, b)), "B*ab+(-1)*B*b+a*ab+(-1)*a*b");
    EXPECT_STR(MUL(V("y"), V("x")), "x*y");
    release();
}

TEST(equality_is_the_canonical_form) {
    const Poly *x = V("x");
    const Poly *y = V("y");
    const Poly *square = POW(ADD(x, y), 2);
    const Poly *expanded = ADD(ADD(POW(x, 2), MULI(2, MUL(x, y))), POW(y, 2));
    EXPECT(poly_equal(square, expanded));
    EXPECT(!poly_equal(ADDI(x, 3), ADDI(y, 3)));
    EXPECT(!poly_equal(x, NEG(x)));
    release();
}

TEST(eval_binds_every_variable_and_ignores_extras) {
    const Poly *x = V("x");
    const Poly *y = V("y");
    const Poly *z = V("z");
    const Poly *p = ADD(POW(SUB(x, MULI(2, y)), 3), MULI(7, z));
    Binding vals[] = {{"x", 3}, {"y", -2}, {"z", 5}, {"w", 9}};
    EXPECT_EQ(poly_eval(p, vals, 4), 378);
    EXPECT_EQ(poly_eval(C(-3), NULL, 0), -3);
    EXPECT_EQ(poly_eval(K(poly_zero()), NULL, 0), 0);
    release();
}

TEST(apply_evaluates_some_variables) {
    const Poly *x = V("x");
    const Poly *y = V("y");
    Binding x3[] = {{"x", 3}};
    EXPECT_STR(K(poly_apply(ADD(MUL(x, y), MULI(2, x)), x3, 1)), "3*y+6");
    release();
}

// The fixtures' polynomials, built as tests/golden's Python built them:
//   (x + 2*y - 3*z + 1) * (x*y - y**2 + 4*z - 5) * (z - x + 7) * (a - x*z)
//   (x - 2*y + z - 1) ** 6
static const Poly *fixture_product(void) {
    const Poly *x = V("x");
    const Poly *y = V("y");
    const Poly *z = V("z");
    const Poly *a = V("a");
    const Poly *f1 = ADDI(SUB(ADD(x, MULI(2, y)), MULI(3, z)), 1);
    const Poly *f2 = SUBI(ADD(SUB(MUL(x, y), POW(y, 2)), MULI(4, z)), 5);
    const Poly *f3 = ADDI(SUB(z, x), 7);
    const Poly *f4 = SUB(a, MUL(x, z));
    return MUL(MUL(MUL(f1, f2), f3), f4);
}

static const Poly *fixture_power(void) {
    const Poly *x = V("x");
    const Poly *y = V("y");
    const Poly *z = V("z");
    return POW(SUBI(ADD(SUB(x, MULI(2, y)), z), 1), 6);
}

TEST(a_product_prints_as_the_python_golden) {
    char *want = golden("tests/golden/poly_product.txt");
    EXPECT(want != NULL);
    const Poly *p = fixture_product();
    EXPECT_EQ(p->len, 54);
    int same = str_is(p, want);
    free(want);
    EXPECT(same);
    Binding vals[] = {{"x", 2}, {"y", 3}, {"z", -1}, {"a", 4}};
    EXPECT_EQ(poly_eval(p, vals, 4), -3456);
    release();
}

TEST(a_power_prints_as_the_python_golden) {
    char *want = golden("tests/golden/poly_power.txt");
    EXPECT(want != NULL);
    const Poly *p = fixture_power();
    EXPECT_EQ(p->len, 84);
    int same = str_is(p, want);
    free(want);
    EXPECT(same);
    Binding vals[] = {{"x", 2}, {"y", -3}, {"z", 5}};
    EXPECT_EQ(poly_eval(p, vals, 3), 2985984);
    release();
}

static void eval_missing_a_variable(void) {
    Poly p = poly_add(V("x"), V("y"));
    Binding vals[] = {{"x", 1}};
    poly_eval(&p, vals, 1);
}

static void apply_an_unknown_variable(void) {
    Binding vals[] = {{"w", 1}};
    poly_apply(V("x"), vals, 1);
}

static void substitute_an_unknown_variable(void) { poly_substitute(V("x"), "w", V("u")); }
static void an_illegal_name(void) { poly_var("x-y"); }
static void a_negative_power(void) { poly_pow(V("x"), -1); }
static void a_coefficient_overflow(void) { poly_pow(ADDI(V("x"), 1000000), 4); }

TEST(the_refusals_stop_with_pythons_words) {
    EXPECT_STOPS(eval_missing_a_variable, "The var y was not supplied.");
    EXPECT_STOPS(apply_an_unknown_variable, "w is not a variable for x");
    EXPECT_STOPS(substitute_an_unknown_variable, "Unknown variable");
    EXPECT_STOPS(an_illegal_name, "Do not use variable names that include -.");
    EXPECT_STOPS(a_negative_power, "we do not support negative exponents");
    EXPECT_STOPS(a_coefficient_overflow, "int64 overflow in mul(");
    release();
}

TEST_MAIN(TEST_CASE(the_docstrings_symbolic_manipulation), TEST_CASE(the_docstrings_multiple_variables),
          TEST_CASE(the_docstrings_substitute), TEST_CASE(signs_print_in_parentheses_as_pythons_do),
          TEST_CASE(zero_one_and_the_shortcuts_print_as_pythons_do),
          TEST_CASE(variables_sort_by_name_bytewise), TEST_CASE(equality_is_the_canonical_form),
          TEST_CASE(eval_binds_every_variable_and_ignores_extras), TEST_CASE(apply_evaluates_some_variables),
          TEST_CASE(a_product_prints_as_the_python_golden), TEST_CASE(a_power_prints_as_the_python_golden),
          TEST_CASE(the_refusals_stop_with_pythons_words))
