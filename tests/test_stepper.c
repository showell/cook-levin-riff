// test_stepper.c: the stepper against stepper.py. The four step polynomials
// print as Python's `str` of them, byte for byte (tests/golden/step_*.txt,
// made by stepper.py itself); the compiled fast path equals `poly_eval` on
// every 0/1 assignment; `step` keeps Python's `halted`; the asserts stop.

#include "test_stops.h"

#include "../src/vm.c"
#include "../src/stepper.c"
#include "../src/poly.c"
#include "../src/ring_int.c"

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

// 1 when p prints as the golden file; says where they part when not.
static int prints_as(const Poly *p, const char *path, size_t want_terms, size_t want_chars) {
    char *want = golden(path);
    if (want == NULL) {
        fprintf(stderr, "  no golden file %s\n", path);
        return 0;
    }
    char *got = poly_str(p);
    size_t i = 0;
    while (got[i] && got[i] == want[i]) i++;
    int same = got[i] == want[i];
    if (!same) fprintf(stderr, "  %s: %zu chars, the golden %zu; first difference at %zu\n", path, strlen(got), strlen(want), i);
    int sizes = p->len == want_terms && strlen(want) == want_chars;
    if (!sizes) fprintf(stderr, "  %s: %zu terms (want %zu), golden %zu chars (want %zu)\n", path, p->len, want_terms, strlen(want), want_chars);
    free(got);
    free(want);
    return same && sizes;
}

TEST(the_step_polynomials_print_as_pythons) {
    const StepPolys *p = step_polynomials();
    EXPECT(prints_as(&p->hb, "tests/golden/step_hb.txt", 786, 36547));
    EXPECT(prints_as(&p->lb, "tests/golden/step_lb.txt", 2979, 147274));
    EXPECT(prints_as(&p->halted, "tests/golden/step_halted.txt", 25, 514));
    EXPECT(prints_as(&p->accepted, "tests/golden/step_accepted.txt", 33, 728));
}

TEST(step_polynomials_are_built_once) {
    EXPECT(step_polynomials() == step_polynomials());
    EXPECT(step_polynomials()->hb.terms == step_polynomials()->hb.terms);
}

TEST(construct_polynomials_on_constants_is_the_step) {
    // The six inputs as constant Polys: each result is that step's value.
    Poly zero = poly_constant(0);
    Poly one = poly_constant(1);
    // AX = 2 (hb 1, lb 0), running, op decr (2: op_hb 1, op_lb 0): AX = 1.
    StepPolys s = construct_polynomials(&one, &zero, &zero, &zero, &one, &zero);
    char *hb = poly_str(&s.hb);
    char *lb = poly_str(&s.lb);
    char *halted = poly_str(&s.halted);
    char *accepted = poly_str(&s.accepted);
    int ok = strcmp(hb, "0") == 0 && strcmp(lb, "1") == 0 && strcmp(halted, "0") == 0 && strcmp(accepted, "0") == 0;
    if (!ok) fprintf(stderr, "  hb %s, lb %s, halted %s, accepted %s\n", hb, lb, halted, accepted);
    free(hb);
    free(lb);
    free(halted);
    free(accepted);
    step_polys_free(&s);
    poly_free(&zero);
    poly_free(&one);
    EXPECT(ok);
}

TEST(the_compiled_polynomials_equal_poly_eval_on_every_assignment) {
    const StepPolys *p = step_polynomials();
    const Poly *polys[4] = {&p->hb, &p->lb, &p->halted, &p->accepted};
    size_t checked = 0;
    size_t differ = 0;
    for (int which = 0; which < 4; which++) {
        CompiledPoly c = compile_step_poly(polys[which]);
        if (c.len != polys[which]->len) differ++;
        for (int bits = 0; bits < 1 << NUM_SLOTS; bits++) {
            int64_t vals[NUM_SLOTS];
            Binding binds[NUM_SLOTS];
            for (int s = 0; s < NUM_SLOTS; s++) {
                vals[s] = (bits >> s) & 1;
                binds[s] = (Binding){SLOT_NAMES[s], vals[s]};
            }
            int64_t fast = compiled_eval(&c, vals);
            int64_t slow = poly_eval(polys[which], binds, NUM_SLOTS);
            if (fast != slow) {
                fprintf(stderr, "  polynomial %d, bits %d: compiled %lld, poly_eval %lld\n", which, bits,
                        (long long)fast, (long long)slow);
                differ++;
            }
            checked++;
        }
        compiled_free(&c);
    }
    EXPECT_EQ(differ, 0);
    EXPECT_EQ(checked, 4 * 64);
}

TEST(step_and_step_compiled_agree_on_every_state_and_op) {
    for (int ax = 0; ax < 4; ax++) {
        for (int halted = 0; halted < 2; halted++) {
            for (int accepted = 0; accepted < 2; accepted++) {
                for (int op = 0; op < NUM_OPS; op++) {
                    StepState a = step(ax, halted, accepted, (Op)op);
                    StepState b = step_compiled(ax, halted, accepted, (Op)op);
                    EXPECT(a.ax == b.ax && a.halted == b.halted && a.accepted == b.accepted);
                }
            }
        }
    }
}

TEST(step_follows_the_ops_table) {
    // nada: nothing; zero at 0 accepts; decr at 0 halts, else AX - 1; mod2.
    StepState s = step(3, 0, 0, OP_NADA);
    EXPECT(s.ax == 3 && !s.halted && !s.accepted);
    s = step(0, 0, 0, OP_ZERO);
    EXPECT(s.ax == 0 && !s.halted && s.accepted);
    s = step(2, 0, 0, OP_ZERO);
    EXPECT(s.ax == 2 && !s.halted && !s.accepted);
    s = step(0, 0, 0, OP_DECR);
    EXPECT(s.ax == 0 && s.halted && !s.accepted);
    s = step(3, 0, 0, OP_DECR);
    EXPECT(s.ax == 2 && !s.halted && !s.accepted);
    s = step(3, 0, 0, OP_MOD2);
    EXPECT(s.ax == 1 && !s.halted && !s.accepted);
    s = step(2, 0, 0, OP_MOD2);
    EXPECT(s.ax == 0 && !s.halted && !s.accepted);
    // Halted: every op is nada.
    s = step(3, 1, 0, OP_DECR);
    EXPECT(s.ax == 3 && s.halted && !s.accepted);
    s = step(0, 1, 0, OP_ZERO);
    EXPECT(s.ax == 0 && s.halted && !s.accepted);
}

TEST(halted_is_pythons_an_accept_does_not_halt) {
    // zero at 0 accepts but leaves halted 0; a decr after sets it, and the
    // verdict stays accepted.
    StepState s = step(0, 0, 0, OP_ZERO);
    EXPECT(s.halted == 0 && s.accepted == 1);
    s = step(s.ax, s.halted, s.accepted, OP_DECR);
    EXPECT(s.ax == 0 && s.halted == 1 && s.accepted == 1);
    Op ops[] = {OP_ZERO, OP_DECR};
    Program p = disassemble(assemble(ops, 2));
    EXPECT_EQ(test_with_stepper(p, 0), 1);
    EXPECT_EQ(test_with_stepper(p, 1), 0);
}

static void step_ax_4(void) { step(4, 0, 0, OP_NADA); }
static void step_halted_2(void) { step(0, 2, 0, OP_NADA); }
static void step_accepted_minus_1(void) { step(0, 0, -1, OP_NADA); }
static void step_op_4(void) { step(0, 0, 0, (Op)4); }
static void compiled_on_a_2(void) {
    CompiledPoly c = compile_step_poly(&step_polynomials()->hb);
    int64_t vals[NUM_SLOTS] = {2, 0, 0, 0, 0, 0};
    compiled_eval(&c, vals);
}
static void compile_an_unknown_variable(void) {
    Poly x = poly_var("x");
    compile_step_poly(&x);
}

TEST(the_asserts_stop) {
    EXPECT_STOPS(step_ax_4, "AX not in [0, 1, 2, 3]: 4");
    EXPECT_STOPS(step_halted_2, "halted not in [0, 1]: 2");
    EXPECT_STOPS(step_accepted_minus_1, "accepted not in [0, 1]: -1");
    EXPECT_STOPS(step_op_4, "op not in [0, 1, 2, 3]: 4");
    EXPECT_STOPS(compiled_on_a_2, "slot hb is 2, not 0 or 1");
    EXPECT_STOPS(compile_an_unknown_variable, "x is not a step variable");
}

TEST_MAIN(TEST_CASE(the_step_polynomials_print_as_pythons), TEST_CASE(step_polynomials_are_built_once),
          TEST_CASE(construct_polynomials_on_constants_is_the_step),
          TEST_CASE(the_compiled_polynomials_equal_poly_eval_on_every_assignment),
          TEST_CASE(step_and_step_compiled_agree_on_every_state_and_op), TEST_CASE(step_follows_the_ops_table),
          TEST_CASE(halted_is_pythons_an_accept_does_not_halt), TEST_CASE(the_asserts_stop))
