// tableau_main.c: Cook's tableau for this machine, as the book shows it:
// seven rows (the start and the six steps) by four columns (one core per
// input), printed as two Markdown tables.
//
//   tableau_main
//
// The first table is the tableau of one program, is_not_tiny's answer
// (decr, mod2, zero, decr, zero, nada), each cell the state the stepper's
// compiled step leaves: AX and the flags. The second is the symbolic tableau
// of the SAT encoding for that puzzle's language with every program bit
// free: each cell names the formula's literals for the four state wires
// (hb, lb, halted, accepted), from sat_encode's trace; 0 and 1 are
// constants, vN is variable N and -vN its complement. book/book.shale holds
// the book's two tables to this output, byte for byte.
// A unity build, as main.c is.

#include "vm.c"
#include "stepper.c"
#include "poly.c"
#include "ring_int.c"
#include "sat.c"

static const Op PROGRAM[MAX_STEPS] = {OP_DECR, OP_MOD2, OP_ZERO, OP_DECR, OP_ZERO, OP_NADA};
// is_not_tiny, {1, 2, 3}: input n accepted sets bit n.
#define LANGUAGE 14

static void cell(StepState s) {
    printf(" AX=%d", s.ax);
    if (s.accepted) printf(", accepted");
    if (s.halted) printf(", halted");
    printf(" |");
}

static void literal(Lit x) {
    if (x == SAT_TRUE) printf("1");
    else if (x == SAT_FALSE) printf("0");
    else if (x > 0) printf("v%d", x);
    else printf("-v%d", -x);
}

int main(void) {
    printf("| step | line | core 0 | core 1 | core 2 | core 3 |\n");
    printf("|---|---|---|---|---|---|\n");
    StepState s[NUM_INPUTS];
    for (int n = 0; n < NUM_INPUTS; n++) s[n] = (StepState){n, 0, 0, 0};
    for (int k = 0; k <= MAX_STEPS; k++) {
        if (k == 0) {
            printf("| start | |");
        } else {
            printf("| %d | `%s` |", k, op_name(PROGRAM[k - 1]));
            for (int n = 0; n < NUM_INPUTS; n++) s[n] = step_compiled(s[n].ax, s[n].halted, s[n].accepted, PROGRAM[k - 1]);
        }
        for (int n = 0; n < NUM_INPUTS; n++) cell(s[n]);
        printf("\n");
    }
    printf("\n");

    SatPuzzle p = sat_puzzle(LANGUAGE);
    Cnf c = sat_encode(&p);
    printf("| step | line's bits | core 0 | core 1 | core 2 | core 3 |\n");
    printf("|---|---|---|---|---|---|\n");
    for (int k = 0; k <= MAX_STEPS; k++) {
        if (k == 0) printf("| start | |");
        else printf("| %d | v%d v%d |", k, sat_op_hb(k), sat_op_lb(k));
        for (int n = 0; n < NUM_INPUTS; n++) {
            SatState t = c.trace[n][k];
            printf(" ");
            literal(t.hb);
            printf(" ");
            literal(t.lb);
            printf(" ");
            literal(t.halted);
            printf(" ");
            literal(t.accepted);
            printf(" |");
        }
        printf("\n");
    }
    sat_free(&c);
    return fflush(stdout) == 0 ? 0 : 1;
}
