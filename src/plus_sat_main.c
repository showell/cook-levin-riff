// plus_sat_main.c: Nibble++'s CNF on the command line (plus_sat.h), for
// sat/plus.shale and sat/plus_search.shale. A unity build, as sat_main.c is.
//
//   plus_sat size K                   the circuit of one run of a K-line
//                                     program: `K GATES VARS CLAUSES`
//   plus_sat claim OPS N [accept]     the circuit for OPS (op names joined by
//                                     commas, K of them) with its 2K program
//                                     bits and AX's four fixed to OPS and N
//                                     by unit clauses; with `accept`, a unit
//                                     clause asks that it accepts. DIMACS on
//                                     stdout, its comment `c accept = L`
//                                     naming the output's literal
//   plus_sat search W K               the search for a K-line program that
//                                     decides W (a language's code, 0 to
//                                     65535, bit n for input n), DIMACS
//   plus_sat search-guarded W K       the search with all 16 copies, each
//                                     copy's output held only when its
//                                     selector is true, and `c guard N S G`
//                                     lines naming them, for
//                                     sat/plus_guided.shale's incremental
//                                     solving under assumptions
//   plus_sat language OPS             the code of the language OPS decides
//                                     on the plain interpreter (plus.c)
//   plus_sat decides W OPS            whether OPS decides W on the plain
//                                     interpreter (plus.c): yes or no, exit
//                                     0 or 1
//   plus_sat census W...              each W's shortest length and its
//                                     witness from the four-op census:
//                                     `W LENGTH OPS` a line (OPS `-` for none)
//   plus_sat census-at L              the same line for every language whose
//                                     shortest length is L

#include "vm.c"
#include "sat.c"
#include "plus.c"
#include "census.c"
#include "plus_sat.c"

#include <string.h>

static int usage(void) {
    fprintf(stderr, "usage: plus_sat size K | claim OPS N [accept] | search W K | search-guarded W K | language OPS | decides W OPS | census W... | census-at L\n");
    return 2;
}

static long number(const char *s, long lo, long hi) {
    if (*s == 0) return -1;
    long v = 0;
    for (const char *t = s; *t; t++) {
        if (*t < '0' || *t > '9' || v > hi) return -1;
        v = 10 * v + (*t - '0');
    }
    return v < lo || v > hi ? -1 : v;
}

// OPS as op names joined by commas, or `-` for no lines. Gives the count,
// or -1 for a name that is no op or too many lines.
static int parse_ops(const char *s, PlusOp *ops) {
    if (strcmp(s, "-") == 0) return 0;
    int n = 0;
    const char *at = s;
    while (1) {
        const char *end = strchr(at, ',');
        size_t len = end ? (size_t)(end - at) : strlen(at);
        int found = -1;
        for (int op = 0; op < PLUS_NUM_OPS; op++) {
            const char *name = plus_op_name((PlusOp)op);
            if (strlen(name) == len && strncmp(name, at, len) == 0) found = op;
        }
        if (found < 0 || n == CENSUS_MAX_LENGTH) return -1;
        ops[n++] = (PlusOp)found;
        if (!end) return n;
        at = end + 1;
    }
}

static void print_ops(FILE *f, const PlusOp *ops, int n) {
    if (n == 0) fprintf(f, "-");
    for (int i = 0; i < n; i++) fprintf(f, "%s%s", i ? "," : "", plus_op_name(ops[i]));
}

static void write_clauses(FILE *f, const Cnf *c) {
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
}

static void write_lines_comment(FILE *f, int k) {
    fprintf(f, "c a model's line i is 2 * v[2i - 1] + v[2i] (half 0, zero 1, decr 2, mod2 3), i in 1..%d\n", k);
}

static int size(int k) {
    Cnf c = sat_new();
    plus_sat_circuit(&c, k);
    printf("%d %zu %d %zu\n", k, c.num_gates, c.num_vars, c.num_clauses);
    sat_free(&c);
    return 0;
}

static int claim(const PlusOp *ops, int k, int n, int accept) {
    Cnf c = sat_new();
    Lit out = plus_sat_circuit(&c, k);
    for (int i = 1; i <= k; i++) {
        sat_unit(&c, ops[i - 1] / 2 ? plus_sat_hb(i) : -plus_sat_hb(i));
        sat_unit(&c, ops[i - 1] % 2 ? plus_sat_lb(i) : -plus_sat_lb(i));
    }
    for (int j = 0; j < PLUS_AX_BITS; j++) sat_unit(&c, (n >> j & 1) ? plus_sat_ax(k, j) : -plus_sat_ax(k, j));
    if (accept) sat_unit(&c, out);
    printf("c the claim: program ");
    print_ops(stdout, ops, k);
    printf(" on input %d%s; one run's circuit, %zu gates\n", n, accept ? " accepts" : "", c.num_gates);
    write_lines_comment(stdout, k);
    printf("c AX's bits 0 to 3 are variables %d %d %d %d\n", plus_sat_ax(k, 0), plus_sat_ax(k, 1), plus_sat_ax(k, 2),
           plus_sat_ax(k, 3));
    printf("c accept = %d\n", out);
    write_clauses(stdout, &c);
    sat_free(&c);
    return 0;
}

static int search(uint32_t lang, int k) {
    Cnf c = plus_sat_search(lang, k);
    printf("c the search: a %d-line Nibble++ program that decides language %u; 16 runs, %zu gates\n", k, lang,
           c.num_gates);
    write_lines_comment(stdout, k);
    write_clauses(stdout, &c);
    sat_free(&c);
    return 0;
}

static int search_guarded(uint32_t lang, int k) {
    Lit goals[PLUS_INPUTS];
    Cnf c = plus_sat_search_guarded(lang, k, goals);
    printf("c the guarded search: a %d-line Nibble++ program that decides language %u; 16 runs, %zu gates, each run's "
           "output held by its selector\n",
           k, lang, c.num_gates);
    write_lines_comment(stdout, k);
    printf("c each `c guard N S G` line: input N's selector is variable S and its goal literal G (true or false for "
           "a constant)\n");
    for (int n = 0; n < PLUS_INPUTS; n++) {
        printf("c guard %d %d ", n, plus_sat_selector(k, n));
        if (goals[n] == SAT_TRUE) {
            printf("true\n");
        } else if (goals[n] == SAT_FALSE) {
            printf("false\n");
        } else {
            printf("%d\n", goals[n]);
        }
    }
    write_clauses(stdout, &c);
    sat_free(&c);
    return 0;
}

// Static: a census holds two 64 Ki tables beside its state arrays.
static Census census;

static void census_line(uint32_t w) {
    PlusOp ops[CENSUS_MAX_LENGTH];
    int len = census_witness(&census, w, ops);
    printf("%u %d ", w, len);
    print_ops(stdout, ops, len);
    printf("\n");
}

static const unsigned ALL_OPS = CENSUS_OP(PLUS_HALF) | CENSUS_OP(PLUS_ZERO) | CENSUS_OP(PLUS_DECR) | CENSUS_OP(PLUS_MOD2);

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    const char *cmd = argv[1];
    int ok = 0;
    if (strcmp(cmd, "size") == 0 && argc == 3) {
        long k = number(argv[2], 0, CENSUS_MAX_LENGTH);
        if (k < 0) return usage();
        ok = size((int)k) == 0;
    } else if (strcmp(cmd, "claim") == 0 && (argc == 4 || (argc == 5 && strcmp(argv[4], "accept") == 0))) {
        PlusOp ops[CENSUS_MAX_LENGTH];
        int k = parse_ops(argv[2], ops);
        long n = number(argv[3], 0, PLUS_INPUTS - 1);
        if (k < 0 || n < 0) return usage();
        ok = claim(ops, k, (int)n, argc == 5) == 0;
    } else if (strcmp(cmd, "search") == 0 && argc == 4) {
        long w = number(argv[2], 0, PLUS_LANGUAGES - 1);
        long k = number(argv[3], 0, CENSUS_MAX_LENGTH);
        if (w < 0 || k < 0) return usage();
        ok = search((uint32_t)w, (int)k) == 0;
    } else if (strcmp(cmd, "search-guarded") == 0 && argc == 4) {
        long w = number(argv[2], 0, PLUS_LANGUAGES - 1);
        long k = number(argv[3], 0, CENSUS_MAX_LENGTH);
        if (w < 0 || k < 0) return usage();
        ok = search_guarded((uint32_t)w, (int)k) == 0;
    } else if (strcmp(cmd, "language") == 0 && argc == 3) {
        PlusOp ops[CENSUS_MAX_LENGTH];
        int k = parse_ops(argv[2], ops);
        if (k < 0) return usage();
        printf("%u\n", plus_language(ops, k));
        ok = 1;
    } else if (strcmp(cmd, "decides") == 0 && argc == 4) {
        PlusOp ops[CENSUS_MAX_LENGTH];
        long w = number(argv[2], 0, PLUS_LANGUAGES - 1);
        int k = parse_ops(argv[3], ops);
        if (w < 0 || k < 0) return usage();
        int yes = plus_language(ops, k) == (uint32_t)w;
        printf("%s\n", yes ? "yes" : "no");
        return yes && fflush(stdout) == 0 ? 0 : 1;
    } else if (strcmp(cmd, "census") == 0 && argc >= 3) {
        for (int a = 2; a < argc; a++) {
            if (number(argv[a], 0, PLUS_LANGUAGES - 1) < 0) return usage();
        }
        census_run(&census, ALL_OPS);
        for (int a = 2; a < argc; a++) census_line((uint32_t)number(argv[a], 0, PLUS_LANGUAGES - 1));
        census_free(&census);
        ok = 1;
    } else if (strcmp(cmd, "census-at") == 0 && argc == 3) {
        long len = number(argv[2], 0, CENSUS_MAX_LENGTH);
        if (len < 0) return usage();
        census_run(&census, ALL_OPS);
        for (uint32_t w = 0; w < PLUS_LANGUAGES; w++) {
            if (census.shortest[w] == len) census_line(w);
        }
        census_free(&census);
        ok = 1;
    } else {
        return usage();
    }
    return ok && fflush(stdout) == 0 ? 0 : 1;
}
