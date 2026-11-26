// plus_sat.c: Nibble++ as a circuit and as CNF (plus_sat.h). Built with
// sat.c, whose gates it uses.

#include "plus_sat.h"

#include <stdlib.h>

static void plus_sat_die(const char *what, long value) {
    fprintf(stderr, "plus_sat: %s: %ld\n", what, value);
    abort();
}

int plus_sat_hb(int i) {
    if (i < 1) plus_sat_die("line not from 1", i);
    return 2 * i - 1;
}

int plus_sat_lb(int i) {
    if (i < 1) plus_sat_die("line not from 1", i);
    return 2 * i;
}

int plus_sat_ax(int k, int j) {
    if (j < 0 || j >= PLUS_AX_BITS) plus_sat_die("AX bit not in 0..3", j);
    return 2 * k + 1 + j;
}

#define NOT(x) sat_not(x)
#define AND(x, y) sat_and(c, (x), (y))
#define OR(x, y) sat_or(c, (x), (y))

static Lit plus_xor(Cnf *c, Lit x, Lit y) {
    Lit left = AND(x, NOT(y));
    Lit right = AND(NOT(x), y);
    return OR(left, right);
}

// in0 when s is 0, in1 when s is 1.
static Lit plus_mux2(Cnf *c, Lit s, Lit in0, Lit in1) {
    Lit left = AND(in0, NOT(s));
    Lit right = AND(in1, s);
    return OR(left, right);
}

// in[op], the op spelled by (h, l).
static Lit plus_mux4(Cnf *c, Lit h, Lit l, const Lit in[PLUS_NUM_OPS]) {
    Lit low = plus_mux2(c, l, in[PLUS_HALF], in[PLUS_ZERO]);
    Lit high = plus_mux2(c, l, in[PLUS_DECR], in[PLUS_MOD2]);
    return plus_mux2(c, h, low, high);
}

PlusSatState plus_sat_step(Cnf *c, PlusSatState s, Lit h, Lit l) {
    const Lit *a = s.ax;
    Lit low_any = OR(a[0], a[1]);
    Lit high_any = OR(a[2], a[3]);
    Lit is_zero = NOT(OR(low_any, high_any));
    Lit live = NOT(s.halted);

    Lit op_zero = AND(NOT(h), l);
    Lit op_stops = plus_xor(c, h, l); // zero or decr
    Lit accepts_now = AND(AND(live, op_zero), is_zero);
    Lit halts_now = AND(AND(live, op_stops), is_zero);

    // decr: bit j flips when every bit below it is 0 (the borrow reaches it).
    Lit decr[PLUS_AX_BITS];
    Lit borrow = SAT_TRUE;
    for (int j = 0; j < PLUS_AX_BITS; j++) {
        decr[j] = plus_xor(c, a[j], borrow);
        borrow = AND(borrow, NOT(a[j]));
    }

    PlusSatState out;
    for (int j = 0; j < PLUS_AX_BITS; j++) {
        Lit in[PLUS_NUM_OPS];
        in[PLUS_HALF] = j + 1 < PLUS_AX_BITS ? a[j + 1] : SAT_FALSE;
        in[PLUS_ZERO] = a[j];
        in[PLUS_DECR] = decr[j];
        in[PLUS_MOD2] = j == 0 ? a[0] : SAT_FALSE;
        out.ax[j] = plus_mux4(c, h, l, in);
    }
    out.accepted = OR(s.accepted, accepts_now);
    out.halted = OR(s.halted, halts_now);
    return out;
}

#undef NOT
#undef AND
#undef OR

Lit plus_sat_circuit(Cnf *c, int k) {
    if (k < 0) plus_sat_die("a program of fewer than 0 lines", k);
    c->num_vars = 2 * k + PLUS_AX_BITS;
    PlusSatState s;
    for (int j = 0; j < PLUS_AX_BITS; j++) s.ax[j] = plus_sat_ax(k, j);
    s.halted = SAT_FALSE;
    s.accepted = SAT_FALSE;
    for (int i = 1; i <= k; i++) s = plus_sat_step(c, s, plus_sat_hb(i), plus_sat_lb(i));
    return s.accepted;
}

Cnf plus_sat_search(uint32_t lang, int k) {
    return plus_sat_search_on(lang, k, PLUS_LANGUAGES - 1);
}

Cnf plus_sat_search_on(uint32_t lang, int k, uint32_t inputs) {
    if (k < 0) plus_sat_die("a program of fewer than 0 lines", k);
    if (lang >= PLUS_LANGUAGES) plus_sat_die("language not in 0..65535", (long)lang);
    if (inputs >= PLUS_LANGUAGES) plus_sat_die("input set not in 0..65535", (long)inputs);
    Cnf c = sat_new();
    // At k = 0 there are no program bits, and every output is a constant;
    // sat.c writes a FALSE unit as (1) and (-1), so variable 1 is kept.
    c.num_vars = k > 0 ? 2 * k : 1;
    Lit outs[PLUS_INPUTS];
    for (int n = 0; n < PLUS_INPUTS; n++) {
        if (!(inputs >> n & 1)) continue;
        PlusSatState s;
        for (int j = 0; j < PLUS_AX_BITS; j++) s.ax[j] = (n >> j & 1) ? SAT_TRUE : SAT_FALSE;
        s.halted = SAT_FALSE;
        s.accepted = SAT_FALSE;
        for (int i = 1; i <= k; i++) s = plus_sat_step(&c, s, plus_sat_hb(i), plus_sat_lb(i));
        outs[n] = s.accepted;
    }
    for (int n = 0; n < PLUS_INPUTS; n++) {
        if (inputs >> n & 1) sat_unit(&c, (lang >> n & 1) ? outs[n] : sat_not(outs[n]));
    }
    return c;
}

void plus_sat_ops(const signed char *bits, int k, PlusOp *ops) {
    for (int i = 1; i <= k; i++) ops[i - 1] = (PlusOp)(2 * bits[plus_sat_hb(i)] + bits[plus_sat_lb(i)]);
}
