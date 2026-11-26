// sat_hand_main.c: the SAT encoding (sat.h) small enough to follow by hand,
// as the book's chapters "Cook-Levin as SAT" and "Solved by hand" show it.
// A unity build, as main.c is.
//
//   sat_hand PROOF    PROOF: the DRAT proof of {0, 2, 3} at length 5
//                     (sat/proofs/13-len5.drat)
//
// It prints eight blocks, a blank line between them, and book/book.shale
// holds the book's text to each, byte for byte:
//
//   1. the AND gate's three clauses on all eight rows of a, b and g;
//   2, 3. the step unit of core 0 at line 1: the gates its state after line
//      1 reaches back through, each with its clauses and the ops of line 1
//      that set it, then that state;
//   4, 5. the same for core 1;
//   6. {0} at length 1 solved by unit propagation from the formula's own
//      unit clauses: how much it fixes, and the clauses that carry input 0's
//      demand up to the program bits;
//   7. each op on line 1 of that puzzle, run through the gates, and the
//      language's unit clauses it breaks;
//   8. PROOF's first 12 distinct lemmas, each literal named.

#include "vm.c"
#include "sat.c"

#include <stdlib.h>
#include <string.h>

static const char *const OPS[NUM_OPS] = {"nada", "zero", "decr", "mod2"};

static const SatGate *gate_of(const Cnf *c, int v) {
    if (v <= SAT_PROGRAM_VARS || v > c->num_vars) return NULL;
    return &c->gates[v - SAT_PROGRAM_VARS - 1];
}

// A literal's value under the program `bits` (bit v - 1 is variable v).
static int value(const Cnf *c, Lit x, uint32_t bits) {
    if (x == SAT_TRUE) return 1;
    if (x == SAT_FALSE) return 0;
    int v = abs(x);
    int b;
    const SatGate *g = gate_of(c, v);
    if (g == NULL) {
        b = (int)(bits >> (v - 1) & 1);
    } else {
        int a = value(c, g->a, bits), d = value(c, g->b, bits);
        b = g->kind == GATE_AND ? a && d : a || d;
    }
    return x > 0 ? b : !b;
}

// The program bits with line k set to op and every other line nada.
static uint32_t line_bits(int k, int op) {
    uint32_t bits = 0;
    if (op / 2) bits |= 1u << (sat_op_hb(k) - 1);
    if (op % 2) bits |= 1u << (sat_op_lb(k) - 1);
    return bits;
}

static void print_lit(Lit x) {
    if (x == SAT_TRUE) printf("1");
    else if (x == SAT_FALSE) printf("0");
    else if (x > 0) printf("v%d", x);
    else printf("-v%d", -x);
}

// The ops of line k for which variable v is 1, when v reads line k's bits
// alone; 0 when it reads any other.
static int reads_line_alone(const Cnf *c, int v, int k) {
    if (v <= SAT_PROGRAM_VARS) return v == sat_op_hb(k) || v == sat_op_lb(k);
    const SatGate *g = gate_of(c, v);
    return reads_line_alone(c, abs(g->a), k) && reads_line_alone(c, abs(g->b), k);
}

static void print_ops_setting(const Cnf *c, int v, int k) {
    int count = 0;
    for (int op = 0; op < NUM_OPS; op++) count += value(c, v, line_bits(k, op));
    int seen = 0;
    for (int op = 0; op < NUM_OPS; op++) {
        if (!value(c, v, line_bits(k, op))) continue;
        seen++;
        printf("%s`%s`", seen == 1 ? "" : seen == count ? " or " : ", ", OPS[op]);
    }
}

// A gate's three clauses, DIMACS, each in backticks.
static void print_clauses(const SatGate *g) {
    int o = g->out;
    if (g->kind == GATE_AND) printf("`%d %d` `%d %d` `%d %d %d`", -o, g->a, -o, g->b, o, -g->a, -g->b);
    else printf("`%d %d` `%d %d` `%d %d %d`", o, -g->a, o, -g->b, -o, g->a, g->b);
}

static void print_gate(const SatGate *g) {
    print_lit(g->a);
    printf(" %s ", g->kind == GATE_AND ? "AND" : "OR");
    print_lit(g->b);
}

// 1. The AND gate's clauses on every row.
static void and_table(void) {
    printf("| a | b | g | `-g a` | `-g b` | `g -a -b` | all three | a AND b |\n");
    printf("|---|---|---|---|---|---|---|---|\n");
    for (int r = 0; r < 8; r++) {
        int a = r >> 2 & 1, b = r >> 1 & 1, g = r & 1;
        int c1 = !g || a, c2 = !g || b, c3 = g || !a || !b;
        printf("| %d | %d | %d | %d | %d | %d | %d | %d |\n", a, b, g, c1, c2, c3, c1 && c2 && c3, a && b);
    }
}

static void mark_cone(const Cnf *c, Lit x, char *in) {
    int v = abs(x);
    const SatGate *g = gate_of(c, v);
    if (x == SAT_TRUE || x == SAT_FALSE || g == NULL || in[v]) return;
    in[v] = 1;
    mark_cone(c, g->a, in);
    mark_cone(c, g->b, in);
}

// 2, 3. Core n's step unit at line 1: every gate its state reaches back
// through, which at line 1 read the line's two bits alone.
static void step_unit(const Cnf *c, int n) {
    char *in = calloc((size_t)c->num_vars + 1, 1);
    const SatState *s = &c->trace[n][1];
    mark_cone(c, s->hb, in);
    mark_cone(c, s->lb, in);
    mark_cone(c, s->halted, in);
    mark_cone(c, s->accepted, in);
    printf("| wire | gate | its clauses | 1 when line 1 is |\n");
    printf("|---|---|---|---|\n");
    for (int v = SAT_PROGRAM_VARS + 1; v <= c->num_vars; v++) {
        if (!in[v]) continue;
        const SatGate *g = gate_of(c, v);
        if (!reads_line_alone(c, v, 1)) {
            fprintf(stderr, "sat_hand: v%d reads past line 1\n", v);
            exit(1);
        }
        printf("| v%d | ", v);
        print_gate(g);
        printf(" | ");
        print_clauses(g);
        printf(" | ");
        print_ops_setting(c, v, 1);
        printf(" |\n");
    }
    printf("\ncore %d after line 1: hb ", n);
    print_lit(s->hb);
    printf(", lb ");
    print_lit(s->lb);
    printf(", halted ");
    print_lit(s->halted);
    printf(", accepted ");
    print_lit(s->accepted);
    printf("\n");
    free(in);
}

// Unit propagation from nothing assigned. val[v] is -1 until fixed;
// reason[v] is the offset in c->lits of the clause that fixed it. Gives 0 on
// a conflict, else 1, and the passes it took.
static int propagate(const Cnf *c, signed char *val, size_t *reason, int *passes) {
    for (int v = 1; v <= c->num_vars; v++) val[v] = -1;
    *passes = 0;
    for (int changed = 1; changed;) {
        changed = 0;
        ++*passes;
        size_t i = 0;
        while (i < c->len) {
            size_t start = i;
            int satisfied = 0, open = 0, last = 0;
            for (; c->lits[i] != 0; i++) {
                int x = c->lits[i];
                if (val[abs(x)] < 0) {
                    open++;
                    last = x;
                } else if (val[abs(x)] == (x > 0)) {
                    satisfied = 1;
                }
            }
            i++;
            if (satisfied) continue;
            if (open == 0) return 0;
            if (open == 1) {
                val[abs(last)] = (signed char)(last > 0);
                reason[abs(last)] = start;
                changed = 1;
            }
        }
    }
    return 1;
}

static void print_clause_at(const Cnf *c, size_t start) {
    printf("`");
    for (size_t i = start; c->lits[i] != 0; i++) printf("%s%d", i > start ? " " : "", c->lits[i]);
    printf("`");
}

static void fixed_by(const Cnf *c, const signed char *val, const size_t *reason, int v, const char *what) {
    printf("v%d = %d by ", v, val[v]);
    print_clause_at(c, reason[v]);
    printf(": %s\n", what);
}

// 4. {0} at length 1, by propagation alone.
static int tiny_by_propagation(void) {
    SatPuzzle p = sat_puzzle(1);
    p.max_len = 1;
    Cnf c = sat_encode(&p);
    signed char *val = malloc((size_t)c.num_vars + 1);
    size_t *reason = malloc(((size_t)c.num_vars + 1) * sizeof *reason);
    int passes;
    if (!propagate(&c, val, reason, &passes)) {
        fprintf(stderr, "sat_hand: {0} at length 1 propagates to a conflict\n");
        return 0;
    }
    int fixed = 0;
    for (int v = 1; v <= c.num_vars; v++) fixed += val[v] >= 0;
    printf("{0} at length 1: %d variables, %zu clauses; unit propagation fixes %d of the variables in %d passes, with 0 conflicts\n",
           c.num_vars, c.num_clauses, fixed, passes);
    char what[64];
    for (int k = MAX_STEPS; k >= 1; k--) {
        Lit a = c.trace[0][k].accepted;
        if (a <= 0 || a == SAT_TRUE) {
            fprintf(stderr, "sat_hand: core 0's accepted after line %d is a constant or a complement\n", k);
            return 0;
        }
        snprintf(what, sizeof what, "core 0's accepted after line %d", k);
        fixed_by(&c, val, reason, a, what);
    }
    fixed_by(&c, val, reason, 1, "line 1's hb");
    fixed_by(&c, val, reason, 2, "line 1's lb");
    uint32_t bits = 0;
    for (int v = 1; v <= SAT_PROGRAM_VARS; v++) bits |= (uint32_t)(val[v] == 1) << (v - 1);
    printf("the program: line 1 is `%s`, lines 2 to 6 `nada`\n", OPS[2 * val[1] + val[2]]);
    int ok = sat_solves(1, sat_program_code(bits));
    free(val);
    free(reason);
    sat_free(&c);
    return ok;
}

// 5. Each op on line 1 of {0} at length 1, through the gates.
static void tiny_each_op(void) {
    SatPuzzle p = sat_puzzle(1);
    p.max_len = 1;
    Cnf c = sat_encode(&p);
    printf("| line 1 | accepted after line 6, inputs 0 1 2 3 | {0} asks for | unit clauses broken |\n");
    printf("|---|---|---|---|\n");
    for (int op = 0; op < NUM_OPS; op++) {
        uint32_t bits = line_bits(1, op);
        printf("| `%s` |", OPS[op]);
        for (int n = 0; n < NUM_INPUTS; n++) printf(" %d", value(&c, c.trace[n][MAX_STEPS].accepted, bits));
        printf(" |");
        for (int n = 0; n < NUM_INPUTS; n++) printf(" %d", n == 0);
        printf(" |");
        int broken = 0;
        for (int n = 0; n < NUM_INPUTS; n++) {
            Lit a = c.trace[n][MAX_STEPS].accepted;
            if (value(&c, a, bits) == (n == 0)) continue;
            printf("%s `%d` (input %d)", broken ? "," : "", n == 0 ? a : -a, n);
            broken++;
        }
        if (broken == 0) printf(" all four hold");
        printf(" |\n");
    }
    sat_free(&c);
}

// What variable v is, in words.
static void describe(const Cnf *c, int v) {
    if (v <= SAT_PROGRAM_VARS) {
        printf("line %d's %s", (v + 1) / 2, v % 2 ? "hb" : "lb");
        return;
    }
    for (int k = 1; k <= MAX_STEPS; k++) {
        for (int n = 0; n < NUM_INPUTS; n++) {
            const SatState *s = &c->trace[n][k];
            const Lit wires[4] = {s->hb, s->lb, s->halted, s->accepted};
            static const char *const names[4] = {"hb", "lb", "halted", "accepted"};
            for (int w = 0; w < 4; w++) {
                if (abs(wires[w]) != v || wires[w] == SAT_TRUE || wires[w] == SAT_FALSE) continue;
                printf("%score %d's %s after line %d", wires[w] > 0 ? "" : "1 minus ", n, names[w], k);
                return;
            }
        }
    }
    for (int k = 1; k <= MAX_STEPS; k++) {
        if (!reads_line_alone(c, v, k)) continue;
        printf("1 when line %d is ", k);
        print_ops_setting(c, v, k);
        return;
    }
    print_gate(gate_of(c, v));
}

// 6. The proof's first distinct lemmas, named.
static int lemmas(const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "sat_hand: cannot read %s\n", path);
        return 0;
    }
    SatPuzzle p = sat_puzzle(13);
    p.max_len = 5;
    Cnf c = sat_encode(&p);
    char line[4096];
    char seen[12][4096];
    int shown = 0;
    while (shown < 12 && fgets(line, sizeof line, f) != NULL) {
        line[strcspn(line, "\n")] = 0;
        if (line[0] == 'd' || line[0] == 0) continue;
        int again = 0;
        for (int s = 0; s < shown; s++) again = again || strcmp(seen[s], line) == 0;
        if (again) continue;
        strcpy(seen[shown++], line);
        int lits[64], n = 0;
        for (char *t = strtok(line, " "); t != NULL; t = strtok(NULL, " ")) {
            int x = atoi(t);
            if (x != 0 && n < 64) lits[n++] = x;
        }
        printf("`%s`: ", seen[shown - 1]);
        for (int i = 0; i < n; i++) {
            printf("%sv%d = %d (", i ? ", or " : "", abs(lits[i]), lits[i] > 0);
            describe(&c, abs(lits[i]));
            printf(")");
        }
        printf("\n");
    }
    fclose(f);
    sat_free(&c);
    return shown == 12;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: sat_hand PROOF\n");
        return 2;
    }
    and_table();
    printf("\n");
    SatPuzzle p = sat_puzzle(1);
    Cnf c = sat_encode(&p);
    step_unit(&c, 0);
    printf("\n");
    step_unit(&c, 1);
    sat_free(&c);
    printf("\n");
    if (!tiny_by_propagation()) return 1;
    printf("\n");
    tiny_each_op();
    printf("\n");
    if (!lemmas(argv[1])) return 1;
    return fflush(stdout) == 0 ? 0 : 1;
}
