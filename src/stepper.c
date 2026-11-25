// stepper.c: the machine's step as polynomials (stepper.h).

#include "stepper.h"

#include "ring_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void stepper_die(const char *what, long value) {
    fprintf(stderr, "stepper: %s: %ld\n", what, value);
    abort();
}

// ── The gates, over a kept list ───────────────────────────────────────────
//
// Every Poly a construction makes is kept here and freed at its end, as an
// arena would: the gates take and give pointers into the list.

typedef struct {
    Poly polys[256];
    size_t len;
} Kept;

static const Poly *keep(Kept *k, Poly p) {
    if (k->len == sizeof k->polys / sizeof k->polys[0]) {
        fprintf(stderr, "stepper: too many Polys kept\n");
        abort();
    }
    k->polys[k->len] = p;
    return &k->polys[k->len++];
}

static void release(Kept *k) {
    for (size_t i = 0; i < k->len; i++) poly_free(&k->polys[i]);
    k->len = 0;
}

static const Poly *VAR(Kept *k, const char *label) { return keep(k, poly_var(label)); }
static const Poly *NOT(Kept *k, const Poly *x) { return keep(k, poly_int_sub(1, x)); }
static const Poly *AND(Kept *k, const Poly *x, const Poly *y) { return keep(k, poly_mul(x, y)); }

// (x + y) - (x * y), in Python's order: the sum, the product, the difference.
static const Poly *OR(Kept *k, const Poly *x, const Poly *y) {
    const Poly *sum = keep(k, poly_add(x, y));
    const Poly *product = keep(k, poly_mul(x, y));
    return keep(k, poly_sub(sum, product));
}

// OR(OR(w, x), OR(y, z)); C leaves an argument list's order open, so the
// two inner ORs are made one after the other, left first, as Python does.
static const Poly *OR4(Kept *k, const Poly *w, const Poly *x, const Poly *y, const Poly *z) {
    const Poly *left = OR(k, w, x);
    const Poly *right = OR(k, y, z);
    return OR(k, left, right);
}

// ── construct_polynomials ─────────────────────────────────────────────────
//
// Line for line stepper.py's; a nested call is spelled as temporaries made
// in Python's order (its arguments left to right).

StepPolys construct_polynomials(const Poly *hb, const Poly *lb, const Poly *halted, const Poly *accepted,
                                const Poly *op_hb, const Poly *op_lb) {
    Kept *k = calloc(1, sizeof *k);
    if (k == NULL) stepper_die("out of memory", 0);
    const Poly *FALSE = keep(k, poly_constant(0));
    const Poly *t;
    const Poly *u;

    const Poly *is_3 = AND(k, hb, lb);
    t = NOT(k, lb);
    const Poly *is_2 = AND(k, hb, t);
    t = NOT(k, hb);
    const Poly *is_1 = AND(k, t, lb);
    t = NOT(k, hb);
    u = NOT(k, lb);
    const Poly *is_0 = AND(k, t, u);

    t = NOT(k, op_hb);
    u = NOT(k, op_lb);
    const Poly *is_pass = AND(k, t, u);
    t = NOT(k, op_hb);
    const Poly *is_check = AND(k, t, op_lb);
    t = NOT(k, op_lb);
    const Poly *is_decr = AND(k, op_hb, t);
    const Poly *is_mod2 = AND(k, op_hb, op_lb);

    const Poly *runs_pass = OR(k, is_pass, halted);
    t = NOT(k, halted);
    const Poly *runs_check = AND(k, is_check, t);
    t = NOT(k, halted);
    const Poly *runs_decr = AND(k, is_decr, t);
    t = NOT(k, halted);
    const Poly *runs_mod2 = AND(k, is_mod2, t);

    const Poly *pass1 = AND(k, is_1, runs_pass);
    const Poly *pass2 = AND(k, is_2, runs_pass);
    const Poly *pass3 = AND(k, is_3, runs_pass);
    const Poly *pass_accepts = FALSE;
    const Poly *pass_halts = FALSE;

    const Poly *check1 = AND(k, is_1, runs_check);
    const Poly *check2 = AND(k, is_2, runs_check);
    const Poly *check3 = AND(k, is_3, runs_check);
    const Poly *check_accepts = AND(k, is_0, runs_check);
    const Poly *check_halts = FALSE;

    const Poly *decr1 = AND(k, is_2, runs_decr);
    const Poly *decr2 = AND(k, is_3, runs_decr);
    const Poly *decr3 = FALSE;
    const Poly *decr_accepts = FALSE;
    const Poly *decr_halts = AND(k, is_0, runs_decr);

    t = OR(k, is_3, is_1);
    const Poly *mod1 = AND(k, t, runs_mod2);
    const Poly *mod2 = FALSE;
    const Poly *mod3 = FALSE;
    const Poly *mod_accepts = FALSE;
    const Poly *mod_halts = FALSE;

    const Poly *newly_accepted = OR4(k, pass_accepts, check_accepts, decr_accepts, mod_accepts);
    const Poly *accepted_out = OR(k, accepted, newly_accepted);

    const Poly *newly_halted = OR4(k, pass_halts, check_halts, decr_halts, mod_halts);
    const Poly *halted_out = OR(k, halted, newly_halted);

    const Poly *becomes_1 = OR4(k, pass1, check1, decr1, mod1);
    const Poly *becomes_2 = OR4(k, pass2, check2, decr2, mod2);
    const Poly *becomes_3 = OR4(k, pass3, check3, decr3, mod3);

    const Poly *hb_set = OR(k, becomes_3, becomes_2);
    const Poly *lb_set = OR(k, becomes_3, becomes_1);

    StepPolys out = {poly_copy(hb_set), poly_copy(lb_set), poly_copy(halted_out), poly_copy(accepted_out)};
    release(k);
    free(k);
    return out;
}

void step_polys_free(StepPolys *s) {
    poly_free(&s->hb);
    poly_free(&s->lb);
    poly_free(&s->halted);
    poly_free(&s->accepted);
}

static StepPolys STEP_POLYNOMIALS;
static int step_polynomials_built;

const StepPolys *step_polynomials(void) {
    if (!step_polynomials_built) {
        Kept *k = calloc(1, sizeof *k);
        if (k == NULL) stepper_die("out of memory", 0);
        const Poly *hb = VAR(k, "hb");
        const Poly *lb = VAR(k, "lb");
        const Poly *halted = VAR(k, "halted");
        const Poly *accepted = VAR(k, "accepted");
        const Poly *op_hb = VAR(k, "op_hb");
        const Poly *op_lb = VAR(k, "op_lb");
        STEP_POLYNOMIALS = construct_polynomials(hb, lb, halted, accepted, op_hb, op_lb);
        release(k);
        free(k);
        step_polynomials_built = 1;
    }
    return &STEP_POLYNOMIALS;
}

// ── step ──────────────────────────────────────────────────────────────────

// Python's four asserts.
static void check_step_args(int ax, int halted, int accepted, Op op) {
    if (ax < 0 || ax > 3) stepper_die("AX not in [0, 1, 2, 3]", ax);
    if (halted != 0 && halted != 1) stepper_die("halted not in [0, 1]", halted);
    if (accepted != 0 && accepted != 1) stepper_die("accepted not in [0, 1]", accepted);
    if ((int)op < 0 || (int)op > 3) stepper_die("op not in [0, 1, 2, 3]", (long)op);
}

// A ring value as one of the bits `step` turns into AX; anything else would
// be a stepper bug, never a wrap.
static int bit_of(int64_t v, const char *what) {
    if (v != 0 && v != 1) {
        fprintf(stderr, "stepper: %s evaluated to %lld, not 0 or 1\n", what, (long long)v);
        abort();
    }
    return (int)v;
}

StepState step(int ax, int halted, int accepted, Op op) {
    check_step_args(ax, halted, accepted, op);
    int hb = ax / 2;
    int lb = ax % 2;
    int op_hb = (int)op / 2;
    int op_lb = (int)op % 2;

    const StepPolys *p = step_polynomials();
    Binding vals[] = {{"hb", hb},           {"lb", lb},       {"halted", halted},
                      {"accepted", accepted}, {"op_hb", op_hb}, {"op_lb", op_lb}};
    size_t n = sizeof vals / sizeof vals[0];
    int hb_set = bit_of(poly_eval(&p->hb, vals, n), "hb_set");
    int lb_set = bit_of(poly_eval(&p->lb, vals, n), "lb_set");
    int new_halted = bit_of(poly_eval(&p->halted, vals, n), "halted");
    int new_accepted = bit_of(poly_eval(&p->accepted, vals, n), "accepted");

    StepState out = {2 * hb_set + lb_set, new_halted, new_accepted, 0};
    return out;
}

int test_with_stepper(Program program, int ax) {
    StepState s = {ax, 0, 0, 0};
    for (int i = 0; i < MAX_STEPS; i++) s = step(s.ax, s.halted, s.accepted, program.ops[i]);
    return s.accepted;
}

// ── The fast path ─────────────────────────────────────────────────────────

static const char *const SLOT_NAMES[NUM_SLOTS] = {"hb", "lb", "halted", "accepted", "op_hb", "op_lb"};

static int slot_of(const char *name) {
    for (int s = 0; s < NUM_SLOTS; s++) {
        if (strcmp(SLOT_NAMES[s], name) == 0) return s;
    }
    fprintf(stderr, "stepper: %s is not a step variable\n", name);
    abort();
    return 0; // not reached; tcc can't see that abort doesn't return
}

CompiledPoly compile_step_poly(const Poly *p) {
    CompiledPoly c = {NULL, p->len};
    c.terms = malloc((p->len ? p->len : 1) * sizeof *c.terms);
    if (c.terms == NULL) stepper_die("out of memory", 0);
    for (size_t i = 0; i < p->len; i++) {
        const Term *t = &p->terms[i];
        uint8_t mask = 0;
        for (uint32_t j = 0; j < t->count; j++) mask |= (uint8_t)(1u << slot_of(p->vps[t->first + j].name));
        c.terms[i] = (CompiledTerm){t->coeff, mask};
    }
    return c;
}

void compiled_free(CompiledPoly *c) {
    free(c->terms);
    c->terms = NULL;
    c->len = 0;
}

int64_t compiled_eval(const CompiledPoly *c, const int64_t vals[NUM_SLOTS]) {
    uint8_t ones = 0;
    for (int s = 0; s < NUM_SLOTS; s++) {
        if (vals[s] != 0 && vals[s] != 1) {
            fprintf(stderr, "stepper: slot %s is %lld, not 0 or 1\n", SLOT_NAMES[s], (long long)vals[s]);
            abort();
        }
        if (vals[s]) ones |= (uint8_t)(1u << s);
    }
    int64_t result = RING_ZERO;
    for (size_t i = 0; i < c->len; i++) {
        if ((c->terms[i].mask & ones) == c->terms[i].mask) result = ring_add(result, c->terms[i].coeff);
    }
    return result;
}

static CompiledPoly COMPILED[4];
static int compiled_built;

static const CompiledPoly *compiled_step_polys(void) {
    if (!compiled_built) {
        const StepPolys *p = step_polynomials();
        COMPILED[0] = compile_step_poly(&p->hb);
        COMPILED[1] = compile_step_poly(&p->lb);
        COMPILED[2] = compile_step_poly(&p->halted);
        COMPILED[3] = compile_step_poly(&p->accepted);
        compiled_built = 1;
    }
    return COMPILED;
}

StepState step_compiled(int ax, int halted, int accepted, Op op) {
    check_step_args(ax, halted, accepted, op);
    const CompiledPoly *c = compiled_step_polys();
    int64_t vals[NUM_SLOTS] = {ax / 2, ax % 2, halted, accepted, (int)op / 2, (int)op % 2};
    int hb_set = bit_of(compiled_eval(&c[0], vals), "hb_set");
    int lb_set = bit_of(compiled_eval(&c[1], vals), "lb_set");
    int new_halted = bit_of(compiled_eval(&c[2], vals), "halted");
    int new_accepted = bit_of(compiled_eval(&c[3], vals), "accepted");
    StepState out = {2 * hb_set + lb_set, new_halted, new_accepted, 0};
    return out;
}

int test_with_compiled_stepper(Program program, int ax) {
    StepState s = {ax, 0, 0, 0};
    for (int i = 0; i < MAX_STEPS; i++) s = step_compiled(s.ax, s.halted, s.accepted, program.ops[i]);
    return s.accepted;
}
