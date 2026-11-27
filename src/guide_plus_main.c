// guide_plus_main.c: the Nibble++ chapters' "is_not_14, here" boxes. The
// formal language is_not_14, {0, ..., 13, 15}, is one of the three that
// need 28 lines, and the chapters follow it as the toy's follow program
// 1689: the census's program, the two theorems' counts, the recursion's
// program, rocsat's program and the 27-line question. Each box is computed
// from the repository's C, a ```text fence titled "is_not_14, here: ...", a
// blank line apart; book/book.shale holds the book's source to each, byte
// for byte. Run from the repository's root: rocsat's box reads its
// committed answers in sat/plus/search.txt.
// A unity build, as main.c is.

#include "vm.c"
#include "sat.c"
#include "plus.c"
#include "plus_sat.c"
#include "census.c"
#include "theory_plus.c"

#include <string.h>

enum { IS_NOT_14 = 0xFFFF & ~(1 << 14) };

// Static: a census holds two 64 Ki tables beside its state arrays.
static Census census;

static void stop(const char *why) {
    fprintf(stderr, "guide_plus_main: %s\n", why);
    exit(1);
}

// A program, seven lines a row, each row led by its first line's number.
static void program_rows(const PlusOp *ops, int len) {
    for (int i = 0; i < len; i++) {
        if (i % 7 == 0) printf("%slines %2d to %2d:", i ? "\n" : "", i + 1, i + 7 < len ? i + 7 : len);
        printf(" %s", plus_op_name(ops[i]));
    }
    printf("\n");
}

// The line on which each input is accepted, or `-` for one rejected: the
// shortest prefix of the program that accepts it.
static void accepted_at(const PlusOp *ops, int len) {
    printf("input        ");
    for (int n = 0; n < PLUS_INPUTS; n++) printf(" %2d", n);
    printf("\naccepted at  ");
    for (int n = 0; n < PLUS_INPUTS; n++) {
        int at = 0;
        for (int k = 1; k <= len && at == 0; k++) {
            if (plus_accepts(n, ops, k)) at = k;
        }
        if (at) printf(" %2d", at);
        else printf("  -");
    }
    printf("\n");
}

static int census_length(unsigned ops, PlusOp *witness) {
    census_run(&census, ops);
    int len = census.shortest[IS_NOT_14];
    if (witness != NULL && census_witness(&census, IS_NOT_14, witness) != len) stop("the census's witness has another length");
    census_free(&census);
    return len;
}

static const unsigned ZERO_DECR = CENSUS_OP(PLUS_ZERO) | CENSUS_OP(PLUS_DECR);
static const unsigned ALL_FOUR = CENSUS_OP(PLUS_ZERO) | CENSUS_OP(PLUS_DECR) | CENSUS_OP(PLUS_MOD2) | CENSUS_OP(PLUS_HALF);

// The census's 28-line program, and where it accepts each input.
static void census_box(void) {
    PlusOp ops[CENSUS_MAX_LENGTH];
    int len = census_length(ALL_FOUR, ops);
    if (plus_language(ops, len) != IS_NOT_14) stop("the census's program decides another language");
    printf("```text is_not_14, here: the census's %d-line program for {0, ..., 13, 15}\n", len);
    program_rows(ops, len);
    accepted_at(ops, len);
    printf("```\n\n");
}

// Theorem 13's count with zero and decr alone, against the census.
static void walk_box(void) {
    int most = 15, size = PLUS_INPUTS - 1;
    int walk = theory_walk_length(IS_NOT_14);
    if (walk != most + size) stop("the walk's length is not max(S) + |S|");
    printf("```text is_not_14, here: zero and decr alone\n");
    printf("max(S) + |S| = %d + %d = %d lines, by Theorem 13\n", most, size, walk);
    printf("the census of {zero, decr}:          %d lines\n", census_length(ZERO_DECR, NULL));
    printf("the census of all four opcodes:      %d lines\n```\n\n", census_length(ALL_FOUR, NULL));
}

// The recursion's program, built by theory_plus.c, and its split lines.
static void recursion_box(void) {
    Theory t;
    theory_build(&t, ALL_FOUR);
    PlusOp ops[PLUS_MAX_LINES * 2];
    int len = theory_witness(&t, IS_NOT_14, ops);
    if (len != theory_shortest(&t, IS_NOT_14) || plus_language(ops, len) != IS_NOT_14) stop("the recursion's program is wrong");
    printf("```text is_not_14, here: the recursion's program, Theorem 15\n");
    printf("its walk alone:       %d lines\n", theory_walk_length(IS_NOT_14));
    printf("the recursion's best: %d lines\n", len);
    program_rows(ops, len);
    printf("its split lines:");
    for (int i = 0; i < len; i++) {
        if (ops[i] == PLUS_HALF || ops[i] == PLUS_MOD2) printf(" %s at line %d", plus_op_name(ops[i]), i + 1);
    }
    printf("\n```\n\n");
    theory_free(&t);
}

// rocsat's program for the case, as sat/plus_search.shale wrote it to
// sat/plus/search.txt, run on the interpreter.
static void rocsat_box(void) {
    FILE *f = fopen("sat/plus/search.txt", "r");
    if (f == NULL) stop("cannot read sat/plus/search.txt");
    static char line[4096];
    char found[4096] = "";
    char unsat[64] = "";
    while (fgets(line, sizeof line, f) != NULL) {
        unsigned w;
        if (sscanf(line, " %u", &w) == 1 && w == IS_NOT_14) {
            char *last = strrchr(line, ' ');
            if (last == NULL) stop("sat/plus/search.txt's case has no program");
            snprintf(found, sizeof found, "%s", last + 1);
            found[strcspn(found, "\n")] = '\0';
            char *u = strstr(line, "UNSAT,");
            if (u == NULL) stop("sat/plus/search.txt's case has no UNSAT");
            snprintf(unsat, sizeof unsat, "UNSATISFIABLE");
        }
    }
    fclose(f);
    if (found[0] == '\0') stop("sat/plus/search.txt has no case for is_not_14");
    PlusOp ops[CENSUS_MAX_LENGTH] = {PLUS_HALF};
    int len = 0;
    for (char *name = strtok(found, ","); name != NULL; name = strtok(NULL, ",")) {
        int op = -1;
        for (int k = 0; k < PLUS_NUM_OPS; k++) {
            if (strcmp(name, plus_op_name((PlusOp)k)) == 0) op = k;
        }
        if (op < 0 || len == CENSUS_MAX_LENGTH) stop("sat/plus/search.txt's program has an unknown op");
        ops[len++] = (PlusOp)op;
    }
    PlusOp census_ops[CENSUS_MAX_LENGTH] = {PLUS_HALF};
    census_length(ALL_FOUR, census_ops);
    printf("```text is_not_14, here: rocsat's answers, from sat/plus/search.txt\n");
    printf("at %d lines: SATISFIABLE, a model whose program bits read\n", len);
    program_rows(ops, len);
    printf("the interpreter: it decides {0, ..., 13, 15}: %s\n", plus_language(ops, len) == IS_NOT_14 ? "yes" : "no");
    printf("lines where it differs from the census's program:");
    int differ = 0;
    for (int i = 0; i < len; i++) {
        if (ops[i] != census_ops[i]) {
            printf(" %d (%s, the census %s)", i + 1, plus_op_name(ops[i]), plus_op_name(census_ops[i]));
            differ++;
        }
    }
    if (differ == 0) printf(" none");
    printf("\nat %d lines: %s\n```\n\n", len - 1, unsat);
}

// The 27-line question as the search CNF writes it.
static void question_box(void) {
    int k = 27;
    Cnf c = plus_sat_search(IS_NOT_14, k);
    printf("```text is_not_14, here: the %d-line question\n", k);
    printf("programs of %d lines: 4^%d = 2^%d\n", k, k, 2 * k);
    printf("plus_sat search %d %d: %d variables, %zu clauses\n", IS_NOT_14, k, c.num_vars, c.num_clauses);
    sat_free(&c);
    census_run(&census, ALL_FOUR);
    printf("the census: %zu states, {0, ..., 13, 15} first at level %d\n```\n\n", census.num_states, census.shortest[IS_NOT_14]);
    census_free(&census);
}

int main(void) {
    census_box();
    walk_box();
    recursion_box();
    rocsat_box();
    question_box();
    return fflush(stdout) == 0 ? 0 : 1;
}
