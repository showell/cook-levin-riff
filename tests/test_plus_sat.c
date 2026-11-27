// test_plus_sat.c: Nibble++'s circuit and CNF (src/plus_sat.c) held to the
// plain interpreter (src/plus.c), with no solver. As tests/test_sat.c does
// for the toy, an assignment of the circuit's inputs is unit-propagated
// through the CNF: each Tseitin variable is a gate's output, forced by its
// three clauses once its inputs are known, so propagation ends with every
// variable determined and every clause satisfied, and the output's value is
// the circuit's answer. The answer must be the interpreter's on every input
// of every program of up to 4 lines, of a few hundred random programs at
// several lengths up to 28, and of every census witness, padded with decr
// to 28 lines, for a sample of the languages. The search CNF is held the
// same way: a witness's bits propagate to a model of its language's search
// at its census length, and are refuted by every language one input away;
// the guarded search's CNF holds a run's output only under its selector.

#include "test.h"

#include "../src/vm.c"
#include "../src/sat.c"
#include "../src/plus.c"
#include "../src/census.c"
#include "../src/plus_sat.c"

#include <stdlib.h>
#include <string.h>

enum { LONGEST = 28 };

// Unit propagation from the first `fixed` variables' values, `given[v]` for
// v in 1..fixed. Gives 1 when it ends with every variable determined and
// every clause satisfied, 0 on a conflict, -1 when a variable is left open.
static int propagate(const Cnf *c, int fixed, const signed char *given, signed char *val) {
    for (int v = 1; v <= c->num_vars; v++) val[v] = v <= fixed ? given[v] : -1;
    int left_open = 1;
    while (left_open) {
        int changed = 0;
        left_open = 0;
        size_t i = 0;
        while (i < c->len) {
            int satisfied = 0, open = 0, last = 0;
            for (; c->lits[i] != 0; i++) {
                int x = c->lits[i];
                int v = abs(x);
                if (val[v] < 0) {
                    open++;
                    last = x;
                } else if (val[v] == (x > 0)) {
                    satisfied = 1;
                }
            }
            i++;
            if (satisfied) continue;
            if (open == 0) return 0;
            if (open == 1) {
                val[abs(last)] = (signed char)(last > 0);
                changed = 1;
            } else {
                left_open = 1;
            }
        }
        if (left_open && !changed) break;
    }
    for (int v = 1; v <= c->num_vars; v++) {
        if (val[v] < 0) return -1;
    }
    return 1;
}

static int lit_value(Lit x, const signed char *val) {
    if (x == SAT_TRUE) return 1;
    if (x == SAT_FALSE) return 0;
    return x > 0 ? val[x] : !val[-x];
}

// The program bits of `ops` as variables 1..2k of `given`.
static void set_program(signed char *given, const PlusOp *ops, int k) {
    for (int i = 1; i <= k; i++) {
        given[plus_sat_hb(i)] = (signed char)(ops[i - 1] / 2);
        given[plus_sat_lb(i)] = (signed char)(ops[i - 1] % 2);
    }
}

// The circuit's answer for `ops` on every input, held to the interpreter's.
static void circuit_agrees(const Cnf *c, Lit out, const PlusOp *ops, int k, signed char *given, signed char *val) {
    set_program(given, ops, k);
    for (int n = 0; n < PLUS_INPUTS; n++) {
        for (int j = 0; j < PLUS_AX_BITS; j++) given[plus_sat_ax(k, j)] = (signed char)(n >> j & 1);
        EXPECT_EQ(propagate(c, 2 * k + PLUS_AX_BITS, given, val), 1);
        if (lit_value(out, val) != plus_accepts(n, ops, k)) fprintf(stderr, "  k %d input %d\n", k, n);
        EXPECT_EQ(lit_value(out, val), plus_accepts(n, ops, k));
    }
}

static uint64_t rng = 0x9e3779b97f4a7c15u;

static unsigned next_random(void) {
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (unsigned)(rng >> 32);
}

TEST(one_step_of_constants_folds_to_the_interpreters_step) {
    // A step on constants makes no gate; its state is the interpreter's
    // after one line, for every AX, flag and op (a halted run is left be).
    for (int ax = 0; ax < PLUS_INPUTS; ax++) {
        for (int op = 0; op < PLUS_NUM_OPS; op++) {
            Cnf c = sat_new();
            PlusSatState s;
            for (int j = 0; j < PLUS_AX_BITS; j++) s.ax[j] = (ax >> j & 1) ? SAT_TRUE : SAT_FALSE;
            s.halted = SAT_FALSE;
            s.accepted = SAT_FALSE;
            PlusSatState t = plus_sat_step(&c, s, op / 2 ? SAT_TRUE : SAT_FALSE, op % 2 ? SAT_TRUE : SAT_FALSE);
            EXPECT_EQ(c.num_gates, 0);
            EXPECT_EQ(c.num_clauses, 0);
            int halts = ax == 0 && (op == PLUS_ZERO || op == PLUS_DECR);
            EXPECT_EQ(t.accepted == SAT_TRUE, ax == 0 && op == PLUS_ZERO);
            EXPECT_EQ(t.halted == SAT_TRUE, halts);
            if (!halts) {
                int want = op == PLUS_HALF ? ax / 2 : op == PLUS_ZERO ? ax : op == PLUS_DECR ? ax - 1 : ax % 2;
                int got = 0;
                for (int j = 0; j < PLUS_AX_BITS; j++) got |= (t.ax[j] == SAT_TRUE) << j;
                EXPECT_EQ(got, want);
            }
            // A halted run's flags stay as they were.
            s.halted = SAT_TRUE;
            s.accepted = ax % 2 ? SAT_TRUE : SAT_FALSE;
            t = plus_sat_step(&c, s, op / 2 ? SAT_TRUE : SAT_FALSE, op % 2 ? SAT_TRUE : SAT_FALSE);
            EXPECT_EQ(t.halted, SAT_TRUE);
            EXPECT_EQ(t.accepted, s.accepted);
            sat_free(&c);
        }
    }
}

TEST(the_circuit_is_the_interpreter_on_every_program_of_up_to_4_lines) {
    long runs = 0;
    for (int k = 0; k <= 4; k++) {
        Cnf c = sat_new();
        Lit out = plus_sat_circuit(&c, k);
        signed char *given = calloc((size_t)c.num_vars + 1, 1);
        signed char *val = malloc((size_t)c.num_vars + 1);
        for (long p = 0; p < 1L << (2 * k); p++) {
            PlusOp ops[4];
            for (int i = 0; i < k; i++) ops[i] = (PlusOp)(p >> (2 * i) & 3);
            circuit_agrees(&c, out, ops, k, given, val);
            runs += PLUS_INPUTS;
        }
        free(given);
        free(val);
        sat_free(&c);
    }
    EXPECT_EQ(runs, 16 * (1 + 4 + 16 + 64 + 256));
}

TEST(the_circuit_is_the_interpreter_on_random_programs_up_to_28_lines) {
    static const int LENGTHS[] = {5, 8, 13, 20, LONGEST};
    for (size_t t = 0; t < sizeof LENGTHS / sizeof LENGTHS[0]; t++) {
        int k = LENGTHS[t];
        Cnf c = sat_new();
        Lit out = plus_sat_circuit(&c, k);
        signed char *given = calloc((size_t)c.num_vars + 1, 1);
        signed char *val = malloc((size_t)c.num_vars + 1);
        for (int p = 0; p < 300; p++) {
            PlusOp ops[LONGEST];
            for (int i = 0; i < k; i++) ops[i] = (PlusOp)(next_random() % 4);
            circuit_agrees(&c, out, ops, k, given, val);
        }
        free(given);
        free(val);
        sat_free(&c);
    }
}

// The circuit at 28 lines: 1,340 gates, so 56 + 4 + 1,340 variables and
// three clauses a gate.
TEST(the_circuit_at_28_lines_has_1340_gates) {
    Cnf c = sat_new();
    plus_sat_circuit(&c, LONGEST);
    EXPECT_EQ(c.num_gates, 1340);
    EXPECT_EQ(c.num_vars, 1400);
    EXPECT_EQ(c.num_clauses, 3 * 1340);
    sat_free(&c);
}

static Census census;

TEST(every_sampled_census_witness_padded_to_28_lines_runs_as_the_interpreter_says) {
    census_run(&census, CENSUS_OP(PLUS_HALF) | CENSUS_OP(PLUS_ZERO) | CENSUS_OP(PLUS_DECR) | CENSUS_OP(PLUS_MOD2));
    Cnf c = sat_new();
    Lit out = plus_sat_circuit(&c, LONGEST);
    signed char *given = calloc((size_t)c.num_vars + 1, 1);
    signed char *val = malloc((size_t)c.num_vars + 1);
    int sampled = 0;
    for (uint32_t y = 0; y < PLUS_LANGUAGES; y += 251) {
        PlusOp ops[LONGEST];
        int len = census_witness(&census, y, ops);
        for (int i = len; i < LONGEST; i++) ops[i] = PLUS_DECR;
        EXPECT_EQ(plus_language(ops, LONGEST), y);
        circuit_agrees(&c, out, ops, LONGEST, given, val);
        sampled++;
    }
    EXPECT_EQ(sampled, 262);
    free(given);
    free(val);
    sat_free(&c);
}

// A witness's bits satisfy its language's search at its census length, and
// a language one input away refutes them, by propagation alone.
static void search_holds_witness(uint32_t y) {
    PlusOp ops[LONGEST];
    int len = census_witness(&census, y, ops);
    if (len == 0) return;
    signed char given[2 * LONGEST + 1] = {0};
    set_program(given, ops, len);
    Cnf c = plus_sat_search(y, len);
    signed char *val = malloc((size_t)c.num_vars + 1);
    EXPECT_EQ(propagate(&c, 2 * len, given, val), 1);
    PlusOp back[LONGEST];
    plus_sat_ops(val, len, back);
    for (int i = 0; i < len; i++) EXPECT_EQ(back[i], ops[i]);
    free(val);
    sat_free(&c);
    uint32_t other = y ^ (1u << (y % PLUS_INPUTS));
    c = plus_sat_search(other, len);
    val = malloc((size_t)c.num_vars + 1);
    EXPECT_EQ(propagate(&c, 2 * len, given, val), 0);
    free(val);
    sat_free(&c);
}

// The guarded search's CNF (incremental solving under assumptions): a
// witness of y, with no selector set, propagates to a model in which every
// goal literal of y's search holds, since the witness decides y; for the
// language one input away, it propagates to a model with that input's
// selector false and the rest true, and to a conflict with all 16 true.
static void search_guarded_holds_witness(uint32_t y) {
    PlusOp ops[LONGEST] = {PLUS_HALF};
    int len = census_witness(&census, y, ops);
    if (len == 0) return;
    signed char given[2 * LONGEST + PLUS_INPUTS + 1] = {0};
    set_program(given, ops, len);
    Lit goals[PLUS_INPUTS];
    Cnf c = plus_sat_search_guarded(y, len, goals);
    EXPECT_EQ(plus_sat_selector(len, 0), 2 * len + 1);
    signed char *val = malloc((size_t)c.num_vars + 1);
    EXPECT_EQ(propagate(&c, 2 * len + PLUS_INPUTS, given, val), 1);
    for (int n = 0; n < PLUS_INPUTS; n++) EXPECT_EQ(lit_value(goals[n], val), 1);
    free(val);
    sat_free(&c);
    int gap = (int)(y % PLUS_INPUTS);
    uint32_t other = y ^ (1u << gap);
    c = plus_sat_search_guarded(other, len, goals);
    val = malloc((size_t)c.num_vars + 1);
    for (int n = 0; n < PLUS_INPUTS; n++) given[plus_sat_selector(len, n)] = n != gap;
    EXPECT_EQ(propagate(&c, 2 * len + PLUS_INPUTS, given, val), 1);
    EXPECT_EQ(lit_value(goals[gap], val), 0);
    given[plus_sat_selector(len, gap)] = 1;
    EXPECT_EQ(propagate(&c, 2 * len + PLUS_INPUTS, given, val), 0);
    free(val);
    sat_free(&c);
}

TEST(the_search_admits_each_sampled_witness_and_refutes_it_one_input_away) {
    // The census ran in the test before.
    EXPECT(census.num_states > 0);
    static const uint32_t STRUCTURED[] = {1u << 15, 0xff00, 0xcccc, 0xffff, 1};
    for (size_t i = 0; i < sizeof STRUCTURED / sizeof STRUCTURED[0]; i++) {
        search_holds_witness(STRUCTURED[i]);
        search_guarded_holds_witness(STRUCTURED[i]);
    }
    for (uint32_t y = 7; y < PLUS_LANGUAGES; y += 4099) {
        search_holds_witness(y);
        search_guarded_holds_witness(y);
    }
    census_free(&census);
}

TEST_MAIN(TEST_CASE(one_step_of_constants_folds_to_the_interpreters_step),
          TEST_CASE(the_circuit_is_the_interpreter_on_every_program_of_up_to_4_lines),
          TEST_CASE(the_circuit_is_the_interpreter_on_random_programs_up_to_28_lines),
          TEST_CASE(the_circuit_at_28_lines_has_1340_gates),
          TEST_CASE(every_sampled_census_witness_padded_to_28_lines_runs_as_the_interpreter_says),
          TEST_CASE(the_search_admits_each_sampled_witness_and_refutes_it_one_input_away))
