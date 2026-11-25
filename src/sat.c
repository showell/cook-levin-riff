// sat.c: a Nibble puzzle as a SAT problem (sat.h).

#include "sat.h"

#include <stdlib.h>

static void sat_die(const char *what, long value) {
    fprintf(stderr, "sat: %s: %ld\n", what, value);
    abort();
}

static void check_line(int i) {
    if (i < 1 || i > MAX_STEPS) sat_die("instruction not in 1..6", i);
}

int sat_op_hb(int i) {
    check_line(i);
    return 2 * i - 1;
}

int sat_op_lb(int i) {
    check_line(i);
    return 2 * i;
}

SatPuzzle sat_puzzle(int lang) {
    SatPuzzle p = {lang, MAX_STEPS, 0, 0};
    return p;
}

Cnf sat_new(void) {
    Cnf c = {0};
    c.num_vars = SAT_PROGRAM_VARS;
    return c;
}

void sat_free(Cnf *c) {
    free(c->lits);
    free(c->gates);
    c->lits = NULL;
    c->gates = NULL;
    c->len = c->cap = c->num_clauses = c->num_gates = c->gates_cap = 0;
}

static int is_const(Lit x) { return x == SAT_TRUE || x == SAT_FALSE; }

static void push_lit(Cnf *c, int lit) {
    if (c->len == c->cap) {
        c->cap = c->cap ? 2 * c->cap : 1024;
        c->lits = realloc(c->lits, c->cap * sizeof *c->lits);
        if (c->lits == NULL) sat_die("out of memory", (long)c->cap);
    }
    c->lits[c->len++] = lit;
}

void sat_clause(Cnf *c, const Lit *lits, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (lits[i] == SAT_TRUE) return;
        if (lits[i] == 0) sat_die("a literal is 0", 0);
    }
    size_t kept = 0;
    for (size_t i = 0; i < n; i++) {
        if (lits[i] == SAT_FALSE) continue;
        push_lit(c, lits[i]);
        kept++;
    }
    if (kept == 0) {
        // The empty clause, as two that contradict.
        push_lit(c, 1);
        push_lit(c, 0);
        push_lit(c, -1);
        c->num_clauses++;
    }
    push_lit(c, 0);
    c->num_clauses++;
}

void sat_unit(Cnf *c, Lit x) { sat_clause(c, &x, 1); }

Lit sat_not(Lit x) { return -x; }

// A gate already made from these inputs, or 0.
static int find_gate(const Cnf *c, GateKind kind, Lit a, Lit b) {
    for (size_t i = 0; i < c->num_gates; i++) {
        const SatGate *g = &c->gates[i];
        if (g->kind == kind && g->a == a && g->b == b) return g->out;
    }
    return 0;
}

static int new_gate(Cnf *c, GateKind kind, Lit a, Lit b) {
    if (c->num_gates == c->gates_cap) {
        c->gates_cap = c->gates_cap ? 2 * c->gates_cap : 256;
        c->gates = realloc(c->gates, c->gates_cap * sizeof *c->gates);
        if (c->gates == NULL) sat_die("out of memory", (long)c->gates_cap);
    }
    int out = ++c->num_vars;
    c->gates[c->num_gates++] = (SatGate){kind, a, b, out};
    return out;
}

Lit sat_and(Cnf *c, Lit x, Lit y) {
    if (x == SAT_FALSE || y == SAT_FALSE) return SAT_FALSE;
    if (x == SAT_TRUE) return y;
    if (y == SAT_TRUE) return x;
    if (x == y) return x;
    if (x == -y) return SAT_FALSE;
    Lit a = x < y ? x : y;
    Lit b = x < y ? y : x;
    int out = find_gate(c, GATE_AND, a, b);
    if (out) return out;
    out = new_gate(c, GATE_AND, a, b);
    Lit c1[2] = {-out, a};
    Lit c2[2] = {-out, b};
    Lit c3[3] = {out, -a, -b};
    sat_clause(c, c1, 2);
    sat_clause(c, c2, 2);
    sat_clause(c, c3, 3);
    return out;
}

Lit sat_or(Cnf *c, Lit x, Lit y) {
    if (x == SAT_TRUE || y == SAT_TRUE) return SAT_TRUE;
    if (x == SAT_FALSE) return y;
    if (y == SAT_FALSE) return x;
    if (x == y) return x;
    if (x == -y) return SAT_TRUE;
    Lit a = x < y ? x : y;
    Lit b = x < y ? y : x;
    int out = find_gate(c, GATE_OR, a, b);
    if (out) return out;
    out = new_gate(c, GATE_OR, a, b);
    Lit c1[2] = {out, -a};
    Lit c2[2] = {out, -b};
    Lit c3[3] = {-out, a, b};
    sat_clause(c, c1, 2);
    sat_clause(c, c2, 2);
    sat_clause(c, c3, 3);
    return out;
}

// ── The step: construct_polynomials (stepper.c), gate for gate ───────────

#define NOT(x) sat_not(x)
#define AND(x, y) sat_and(c, (x), (y))
#define OR(x, y) sat_or(c, (x), (y))

// OR(OR(w, x), OR(y, z)), the inner two made left first, as stepper.c's.
static Lit sat_or4(Cnf *c, Lit w, Lit x, Lit y, Lit z) {
    Lit left = OR(w, x);
    Lit right = OR(y, z);
    return OR(left, right);
}

SatState sat_step(Cnf *c, SatState s, Lit op_hb, Lit op_lb) {
    const Lit FALSE = SAT_FALSE;
    Lit hb = s.hb, lb = s.lb, halted = s.halted, accepted = s.accepted;

    Lit is_3 = AND(hb, lb);
    Lit is_2 = AND(hb, NOT(lb));
    Lit is_1 = AND(NOT(hb), lb);
    Lit is_0 = AND(NOT(hb), NOT(lb));

    Lit is_pass = AND(NOT(op_hb), NOT(op_lb));
    Lit is_check = AND(NOT(op_hb), op_lb);
    Lit is_decr = AND(op_hb, NOT(op_lb));
    Lit is_mod2 = AND(op_hb, op_lb);

    Lit runs_pass = OR(is_pass, halted);
    Lit runs_check = AND(is_check, NOT(halted));
    Lit runs_decr = AND(is_decr, NOT(halted));
    Lit runs_mod2 = AND(is_mod2, NOT(halted));

    Lit pass1 = AND(is_1, runs_pass);
    Lit pass2 = AND(is_2, runs_pass);
    Lit pass3 = AND(is_3, runs_pass);
    Lit pass_accepts = FALSE;
    Lit pass_halts = FALSE;

    Lit check1 = AND(is_1, runs_check);
    Lit check2 = AND(is_2, runs_check);
    Lit check3 = AND(is_3, runs_check);
    Lit check_accepts = AND(is_0, runs_check);
    Lit check_halts = FALSE;

    Lit decr1 = AND(is_2, runs_decr);
    Lit decr2 = AND(is_3, runs_decr);
    Lit decr3 = FALSE;
    Lit decr_accepts = FALSE;
    Lit decr_halts = AND(is_0, runs_decr);

    Lit mod1 = AND(OR(is_3, is_1), runs_mod2);
    Lit mod2 = FALSE;
    Lit mod3 = FALSE;
    Lit mod_accepts = FALSE;
    Lit mod_halts = FALSE;

    Lit newly_accepted = sat_or4(c, pass_accepts, check_accepts, decr_accepts, mod_accepts);
    Lit accepted_out = OR(accepted, newly_accepted);

    Lit newly_halted = sat_or4(c, pass_halts, check_halts, decr_halts, mod_halts);
    Lit halted_out = OR(halted, newly_halted);

    Lit becomes_1 = sat_or4(c, pass1, check1, decr1, mod1);
    Lit becomes_2 = sat_or4(c, pass2, check2, decr2, mod2);
    Lit becomes_3 = sat_or4(c, pass3, check3, decr3, mod3);

    Lit hb_set = OR(becomes_3, becomes_2);
    Lit lb_set = OR(becomes_3, becomes_1);

    SatState out = {hb_set, lb_set, halted_out, accepted_out};
    return out;
}

#undef NOT
#undef AND
#undef OR

// ── The puzzle ───────────────────────────────────────────────────────────

static Lit constant(int bit) { return bit ? SAT_TRUE : SAT_FALSE; }

Cnf sat_encode(const SatPuzzle *p) {
    if (p->lang < 0 || p->lang >= NUM_LANGUAGES) sat_die("language not in 0..15", p->lang);
    if (p->max_len < 0 || p->max_len > MAX_STEPS) sat_die("length bound not in 0..6", p->max_len);
    if (p->fixed < 0 || p->fixed >> MAX_STEPS) sat_die("fixed lines past line 6", p->fixed);
    Cnf c = sat_new();
    // The locks first, so a program that breaks one is refuted at once.
    Program fixed = disassemble(p->fixed_ops);
    for (int i = 1; i <= MAX_STEPS; i++) {
        if (i > p->max_len) {
            sat_unit(&c, -sat_op_hb(i));
            sat_unit(&c, -sat_op_lb(i));
        }
        if (p->fixed >> (i - 1) & 1) {
            int op = (int)fixed.ops[i - 1];
            sat_unit(&c, op / 2 ? sat_op_hb(i) : -sat_op_hb(i));
            sat_unit(&c, op % 2 ? sat_op_lb(i) : -sat_op_lb(i));
        }
    }
    for (int n = 0; n < NUM_INPUTS; n++) {
        SatState s = {constant(n / 2), constant(n % 2), SAT_FALSE, SAT_FALSE};
        c.trace[n][0] = s;
        for (int i = 1; i <= MAX_STEPS; i++) {
            s = sat_step(&c, s, sat_op_hb(i), sat_op_lb(i));
            c.trace[n][i] = s;
        }
    }
    for (int n = 0; n < NUM_INPUTS; n++) {
        Lit accepted = c.trace[n][MAX_STEPS].accepted;
        sat_unit(&c, (p->lang >> n & 1) ? accepted : -accepted);
    }
    return c;
}

static void write_lit(FILE *f, Lit x) {
    if (x == SAT_TRUE) {
        fprintf(f, "T");
    } else if (x == SAT_FALSE) {
        fprintf(f, "F");
    } else {
        fprintf(f, "%d", x);
    }
}

int sat_write_dimacs(FILE *f, const Cnf *c, const SatPuzzle *p) {
    Lang lang = language((uint32_t)p->lang);
    fprintf(f, "c Nibble puzzle: accept exactly {");
    for (size_t k = 0; k < lang.len; k++) fprintf(f, "%s%d", k ? ", " : "", lang.items[k]);
    fprintf(f, "} (language %d), lines past %d nada, fixed lines mask %d of program %u\n", p->lang, p->max_len,
            p->fixed, p->fixed_ops);
    fprintf(f, "c a model's op i is 2 * v[2i - 1] + v[2i]:");
    for (int i = 1; i <= MAX_STEPS; i++) fprintf(f, " op%d %d %d", i, sat_op_hb(i), sat_op_lb(i));
    fprintf(f, "\n");
    for (int n = 0; n < NUM_INPUTS; n++) {
        for (int k = 1; k <= MAX_STEPS; k++) {
            const SatState *s = &c->trace[n][k];
            fprintf(f, "c input %d after step %d: hb ", n, k);
            write_lit(f, s->hb);
            fprintf(f, " lb ");
            write_lit(f, s->lb);
            fprintf(f, " halted ");
            write_lit(f, s->halted);
            fprintf(f, " accepted ");
            write_lit(f, s->accepted);
            fprintf(f, "\n");
        }
    }
    fprintf(f, "p cnf %d %zu\n", c->num_vars, c->num_clauses);
    int first = 1;
    for (size_t i = 0; i < c->len; i++) {
        if (c->lits[i] == 0) {
            fprintf(f, first ? "0\n" : " 0\n");
            first = 1;
        } else {
            fprintf(f, first ? "%d" : " %d", c->lits[i]);
            first = 0;
        }
    }
    return ferror(f) == 0;
}

uint32_t sat_program_code(uint32_t bits) {
    uint32_t code = 0;
    uint32_t place = 1;
    for (int i = 1; i <= MAX_STEPS; i++) {
        uint32_t hb = bits >> (sat_op_hb(i) - 1) & 1;
        uint32_t lb = bits >> (sat_op_lb(i) - 1) & 1;
        code += (2 * hb + lb) * place;
        place *= NUM_OPS;
    }
    return code;
}

uint32_t sat_program_bits(uint32_t code) {
    Program p = disassemble(code);
    uint32_t bits = 0;
    for (int i = 1; i <= MAX_STEPS; i++) {
        int op = (int)p.ops[i - 1];
        bits |= (uint32_t)(op / 2) << (sat_op_hb(i) - 1);
        bits |= (uint32_t)(op % 2) << (sat_op_lb(i) - 1);
    }
    return bits;
}

int sat_solves(int lang, uint32_t code) {
    return (int)encoded_language(get_language_that_program_accepts(disassemble(code))) == lang;
}
