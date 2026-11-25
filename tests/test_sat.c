// test_sat.c: the SAT encoding (sat.h), proved with no solver. The step's
// gates are the stepper's on all 64 of its inputs; a model's 12 program
// bits are compose.h's numbering; and for every puzzle, every one of the
// 4,096 assignments of the program bits is unit-propagated through the CNF.
// Unit propagation alone decides it: each Tseitin variable is a gate's
// output, its three clauses force it once its inputs are known, and the
// gates come in the order they were made, inputs first. An assignment the
// propagation refutes is a program the CNF excludes; one it does not
// refute ends with every variable determined and every clause satisfied,
// a model. The models are exactly the programs the machine says solve the
// puzzle, 4,096 in all over the 16 languages; and with lines past N forced
// to nada, exactly those of length N or less, so the CNF is satisfiable
// exactly when N is at least the shortest length.

#include "test.h"

#include "../src/vm.c"
#include "../src/sat.c"
#include "../src/stepper.c"
#include "../src/compose.c"
#include "../src/boolring.c"
#include "../src/poly.c"
#include "../src/ring_int.c"

#include <stdlib.h>

// find_solutions.txt's counts, by language code.
static const int COUNTS[NUM_LANGUAGES] = {844, 681, 303, 172, 248, 883, 74, 13, 63, 12, 520, 150, 13, 1, 15, 104};

static int length_of(uint32_t code) {
    Program p = disassemble(code);
    int len = MAX_STEPS;
    while (len > 0 && p.ops[len - 1] == OP_NADA) len--;
    return len;
}

// Each program's language, as the machine runs it, made once.
static int LANGS[NUM_PROGRAMS];
static int langs_made;

static int lang_of(uint32_t n) {
    if (!langs_made) {
        for (uint32_t k = 0; k < NUM_PROGRAMS; k++) {
            LANGS[k] = (int)encoded_language(get_language_that_program_accepts(disassemble(k)));
        }
        langs_made = 1;
    }
    return LANGS[n];
}

static int bit_of_const(Lit x) {
    if (x == SAT_TRUE) return 1;
    if (x == SAT_FALSE) return 0;
    fprintf(stderr, "  literal %d is not a constant\n", x);
    return -1;
}

TEST(the_steps_gates_are_the_stepper_on_all_64_inputs) {
    for (int v = 0; v < 64; v++) {
        int ax = v & 3, halted = v >> 2 & 1, accepted = v >> 3 & 1, op = v >> 4;
        Cnf c = sat_new();
        SatState s = {ax / 2 ? SAT_TRUE : SAT_FALSE, ax % 2 ? SAT_TRUE : SAT_FALSE, halted ? SAT_TRUE : SAT_FALSE,
                      accepted ? SAT_TRUE : SAT_FALSE};
        SatState out = sat_step(&c, s, op / 2 ? SAT_TRUE : SAT_FALSE, op % 2 ? SAT_TRUE : SAT_FALSE);
        // Every gate of constants folds: nothing is made.
        EXPECT_EQ(c.num_gates, 0);
        EXPECT_EQ(c.num_clauses, 0);
        StepState want = step(ax, halted, accepted, (Op)op);
        EXPECT_EQ(2 * bit_of_const(out.hb) + bit_of_const(out.lb), want.ax);
        EXPECT_EQ(bit_of_const(out.halted), want.halted);
        EXPECT_EQ(bit_of_const(out.accepted), want.accepted);
        sat_free(&c);
    }
}

TEST(a_models_program_bits_are_composes_numbering) {
    for (uint32_t n = 0; n < NUM_PROGRAMS; n++) {
        uint32_t bits = sat_program_bits(n);
        EXPECT_EQ(sat_program_code(bits), n);
        // compose's bit 2i is op i's hb, sat's variable 2i - 1 (bit 2i - 2).
        EXPECT_EQ(compose_ones(0, disassemble(n)) >> 2, bits);
    }
    EXPECT_EQ(sat_op_hb(1), 1);
    EXPECT_EQ(sat_op_lb(6), 12);
}

// Unit propagation from a program's bits. Gives 1 when it ends with every
// variable determined and every clause satisfied, 0 on a conflict, and -1
// when it ends with neither (a variable the program does not determine).
// A pass visits every clause; one found satisfied stays so, since nothing
// is unassigned, so a pass that leaves no clause open has checked them all.
// The gates' order makes one pass do: each gate's inputs are known before
// its clauses come.
static int propagate(const Cnf *c, uint32_t bits, signed char *val) {
    for (int v = 1; v <= c->num_vars; v++) val[v] = -1;
    for (int v = 1; v <= SAT_PROGRAM_VARS; v++) val[v] = (signed char)(bits >> (v - 1) & 1);
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

// Each language's CNF, unbounded and unfixed, propagated from every
// program's bits: propagate's answer, made once a language.
static signed char BASE[NUM_LANGUAGES][NUM_PROGRAMS];
static int base_made[NUM_LANGUAGES];

static int base_verdict(int lang, uint32_t n) {
    if (!base_made[lang]) {
        SatPuzzle p = sat_puzzle(lang);
        Cnf c = sat_encode(&p);
        signed char *val = malloc((size_t)c.num_vars + 1);
        for (uint32_t k = 0; k < NUM_PROGRAMS; k++) BASE[lang][k] = (signed char)propagate(&c, sat_program_bits(k), val);
        free(val);
        sat_free(&c);
        base_made[lang] = 1;
    }
    return BASE[lang][n];
}

// The unit clauses a bounded or fixed puzzle's CNF opens with, on program
// variables, before the unbounded CNF's clauses, which follow them exactly
// (held here). So the program bits decide those units, and propagation from
// them past the units is the unbounded CNF's: a program is admitted when it
// keeps every unit and the unbounded CNF admits it. Gives the units' count.
static size_t lock_units(const Cnf *c, const Cnf *base) {
    size_t units = 0;
    while (2 * units + 1 < c->len && c->lits[2 * units + 1] == 0 && abs(c->lits[2 * units]) <= SAT_PROGRAM_VARS) units++;
    EXPECT_EQ(c->len, base->len + 2 * units);
    EXPECT_EQ(c->num_clauses, base->num_clauses + units);
    EXPECT_EQ(c->num_vars, base->num_vars);
    EXPECT(memcmp(c->lits + 2 * units, base->lits, base->len * sizeof *base->lits) == 0);
    return units;
}

static int keeps_units(const Cnf *c, size_t units, uint32_t bits) {
    for (size_t u = 0; u < units; u++) {
        int x = c->lits[2 * u];
        if ((int)(bits >> (abs(x) - 1) & 1) != (x > 0)) return 0;
    }
    return 1;
}

// The programs the CNF of p admits, held to the machine's: admitted exactly
// when `want(n)`. Gives how many it admits.
static int admitted(const SatPuzzle *p, int (*want)(const SatPuzzle *, uint32_t)) {
    Cnf c = sat_encode(p);
    SatPuzzle unbounded = sat_puzzle(p->lang);
    Cnf base = sat_encode(&unbounded);
    size_t units = lock_units(&c, &base);
    int count = 0, differ = 0, open = 0;
    for (uint32_t n = 0; n < NUM_PROGRAMS; n++) {
        uint32_t bits = sat_program_bits(n);
        int r = keeps_units(&c, units, bits) ? base_verdict(p->lang, n) : 0;
        if (r < 0) open++;
        if ((r == 1) != want(p, n)) {
            if (differ < 3) fprintf(stderr, "  language %d len %d program %u: CNF %d\n", p->lang, p->max_len, n, r);
            differ++;
        }
        count += r == 1;
    }
    sat_free(&base);
    sat_free(&c);
    EXPECT_EQ(open, 0);
    EXPECT_EQ(differ, 0);
    return count;
}

static int solves(const SatPuzzle *p, uint32_t n) { return lang_of(n) == p->lang; }

static int solves_within(const SatPuzzle *p, uint32_t n) {
    return lang_of(n) == p->lang && length_of(n) <= p->max_len;
}

static int solves_fixed(const SatPuzzle *p, uint32_t n) {
    Program a = disassemble(n), b = disassemble(p->fixed_ops);
    for (int k = 0; k < MAX_STEPS; k++) {
        if ((p->fixed >> k & 1) && a.ops[k] != b.ops[k]) return 0;
    }
    return solves_within(p, n);
}

TEST(every_puzzles_models_are_exactly_its_solving_programs) {
    int total = 0;
    for (int y = 0; y < NUM_LANGUAGES; y++) {
        SatPuzzle p = sat_puzzle(y);
        int count = admitted(&p, solves);
        EXPECT_EQ(count, COUNTS[y]);
        total += count;
    }
    EXPECT_EQ(total, NUM_PROGRAMS);
}

TEST(a_length_bound_admits_the_solving_programs_that_short) {
    for (int y = 0; y < NUM_LANGUAGES; y++) {
        int shortest = MAX_STEPS + 1;
        for (uint32_t n = 0; n < NUM_PROGRAMS; n++) {
            if (lang_of(n) == y && length_of(n) < shortest) shortest = length_of(n);
        }
        EXPECT(shortest <= MAX_STEPS);
        for (int len = 0; len <= MAX_STEPS; len++) {
            SatPuzzle p = sat_puzzle(y);
            p.max_len = len;
            int count = admitted(&p, solves_within);
            // Satisfiable exactly when the bound reaches the shortest.
            EXPECT_EQ(count > 0, len >= shortest);
        }
    }
}

TEST(a_fixed_line_admits_the_solving_programs_with_its_op) {
    for (int y = 0; y < NUM_LANGUAGES; y++) {
        for (int k = 1; k <= MAX_STEPS; k++) {
            for (int op = 0; op < NUM_OPS; op++) {
                SatPuzzle p = sat_puzzle(y);
                uint32_t place = 1;
                for (int i = 1; i < k; i++) place *= NUM_OPS;
                p.fixed = 1 << (k - 1);
                p.fixed_ops = (uint32_t)op * place;
                admitted(&p, solves_fixed);
            }
        }
    }
}

TEST(the_empty_clause_is_written_as_two_that_contradict) {
    Cnf c = sat_new();
    Lit none[1] = {SAT_FALSE};
    sat_clause(&c, none, 1);
    EXPECT_EQ(c.num_clauses, 2);
    EXPECT_EQ(c.len, 4);
    EXPECT_EQ(c.lits[0], 1);
    EXPECT_EQ(c.lits[2], -1);
    Lit always[2] = {3, SAT_TRUE};
    sat_clause(&c, always, 2);
    EXPECT_EQ(c.num_clauses, 2);
    sat_free(&c);
}

TEST_MAIN(TEST_CASE(the_steps_gates_are_the_stepper_on_all_64_inputs),
          TEST_CASE(a_models_program_bits_are_composes_numbering),
          TEST_CASE(every_puzzles_models_are_exactly_its_solving_programs),
          TEST_CASE(a_length_bound_admits_the_solving_programs_that_short),
          TEST_CASE(a_fixed_line_admits_the_solving_programs_with_its_op),
          TEST_CASE(the_empty_clause_is_written_as_two_that_contradict))
