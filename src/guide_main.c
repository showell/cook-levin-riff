// guide_main.c: the book's "1689, here" boxes. Program 1689 (zero, decr,
// decr, zero, decr, zero) is the one program that decides {0, 2, 3}, and the
// book follows it through every form it gives the machine: a run, the gates,
// the formula, the circuit, the tableau, the polynomials and the CNF. Each
// box here is computed from the repository's C, a ```text fence titled
// "1689, here: ...", a blank line apart; book/book.shale holds the book's
// source to each, byte for byte. Run from the repository's root: the SAT
// box reads rocsat's committed answer and proof in sat/proofs/.
// A unity build, as main.c is.

#include "vm.c"
#include "stepper.c"
#include "compose.c"
#include "boolring.c"
#include "poly.c"
#include "ring_int.c"
#include "sat.c"
#include "circuit.c"

#include <string.h>

static const Op GUIDE[MAX_STEPS] = {OP_ZERO, OP_DECR, OP_DECR, OP_ZERO, OP_DECR, OP_ZERO};
enum { GUIDE_NUMBER = 1689, GUIDE_LANGUAGE = 13 };

static Net NET;
static int VALUES[CIRCUIT_MAX_WIRES];

static void stop(const char *why) {
    fprintf(stderr, "guide_main: %s\n", why);
    exit(1);
}

static Program guide(void) {
    Program p;
    for (int i = 0; i < MAX_STEPS; i++) p.ops[i] = GUIDE[i];
    return p;
}

// The program's number as the book defines it: line 1 the highest base-4
// digit.
static int book_number(const Op *ops) {
    int n = 0;
    for (int i = 0; i < MAX_STEPS; i++) n = n * 4 + (int)ops[i];
    return n;
}

static void machine_code(const Op *ops) {
    for (int i = 0; i < MAX_STEPS; i++) printf("%s%d%d", i ? " " : "", (int)ops[i] / 2, (int)ops[i] % 2);
}

static void op_list(const Op *ops) {
    for (int i = 0; i < MAX_STEPS; i++) printf("%s%s", i ? ", " : "", op_name(ops[i]));
}

static void print_set(uint32_t code) {
    printf("{");
    int first = 1;
    for (int n = 0; n < NUM_INPUTS; n++) {
        if (code >> n & 1) {
            printf("%s%d", first ? "" : ", ", n);
            first = 0;
        }
    }
    printf("}");
}

// The run on each input: AX after each line, the verdict on the line that
// gives it, and a dot once the run has ended.
static void run_box(void) {
    printf("```text 1689, here: the program, and its run from each input\n");
    printf("program %d: base-4 digits", GUIDE_NUMBER);
    for (int i = 0; i < MAX_STEPS; i++) printf(" %d", (int)GUIDE[i]);
    printf(", machine code ");
    machine_code(GUIDE);
    printf("\nline   op    from 0  from 1  from 2  from 3\n");
    printf("start        AX=0    AX=1    AX=2    AX=3\n");
    Machine m[NUM_INPUTS];
    for (int n = 0; n < NUM_INPUTS; n++) m[n] = vm_start(n);
    for (int k = 0; k < MAX_STEPS; k++) {
        printf("%-6d %-5s", k + 1, op_name(GUIDE[k]));
        for (int n = 0; n < NUM_INPUTS; n++) {
            int was = m[n].halted;
            m[n] = vm_step(m[n], GUIDE[k]);
            const char *cell = "·";
            char ax[8];
            if (!was && m[n].halted) cell = m[n].status == STATUS_ACCEPTED ? "accept" : "reject";
            else if (!was) {
                snprintf(ax, sizeof ax, "AX=%d", m[n].ax);
                cell = ax;
            }
            int pad = n < NUM_INPUTS - 1 ? (int)(7 - (strcmp(cell, "·") == 0 ? 1 : strlen(cell))) : 0;
            printf(" %s%*s", cell, pad, "");
        }
        printf("\n");
    }
    printf("decides ");
    print_set(encoded_language(get_language_that_program_accepts(guide())));
    printf("\n```\n\n");
}

// The sandbox's two lines for the program, as web/page_check.mjs reads them
// off the page: the machine code, the number and the problem's name and set,
// then each input's result.
static void sandbox_box(void) {
    printf("```text 1689, here: the sandbox, with 1689 typed in\n");
    machine_code(GUIDE);
    printf("  (#%d) | is_not_one ", GUIDE_NUMBER);
    print_set(encoded_language(get_language_that_program_accepts(guide())));
    printf("\n");
    for (int n = 0; n < NUM_INPUTS; n++) {
        printf("%s%s", n ? " | " : "", run_progam(n, guide()) == STATUS_ACCEPTED ? "accept" : "reject");
    }
    printf("\n```\n\n");
}

// One fixed bank of gates, the circuit's stepper, ticked once a line from
// AX = 3, in the layout of python/steps.py's run in "The Boolean view".
static void gates_box(void) {
    circuit_stepper(&NET);
    int hb = net_port_wire(&NET, "", "hb'"), lb = net_port_wire(&NET, "", "lb'");
    int halted = net_port_wire(&NET, "", "halted'"), accepted = net_port_wire(&NET, "", "accepted'");
    int s[4] = {1, 1, 0, 0};
    printf("```text 1689, here: one bank of gates, ticked six times from AX = 3\n");
    printf("                hb lb halted accepted\n");
    printf("start           %2d %2d %6d %8d\n", s[0], s[1], s[2], s[3]);
    for (int k = 0; k < MAX_STEPS; k++) {
        int in[6] = {s[0], s[1], s[2], s[3], (int)GUIDE[k] / 2, (int)GUIDE[k] % 2};
        net_eval(&NET, in, VALUES);
        s[0] = VALUES[hb];
        s[1] = VALUES[lb];
        s[2] = VALUES[halted];
        s[3] = VALUES[accepted];
        printf("step %d  %s %d%d %2d %2d %6d %8d\n", k + 1, op_name(GUIDE[k]), (int)GUIDE[k] / 2, (int)GUIDE[k] % 2, s[0],
               s[1], s[2], s[3]);
    }
    if (s[3] != (run_progam(3, guide()) == STATUS_ACCEPTED)) stop("the gates differ from the machine at AX = 3");
    printf("```\n\n");
}

// f, the machine as one function of 14 bits, at the program's four points.
static void formula_box(void) {
    printf("```text 1689, here: f at its four points\n");
    printf("v1 .. v12          v13 v14  f\n");
    for (int n = 0; n < NUM_INPUTS; n++) {
        machine_code(GUIDE);
        printf("   %d   %d  %d\n", n / 2, n % 2, run_progam(n, guide()) == STATUS_ACCEPTED);
    }
    printf("```\n\n");
}

// The decider's 16 inputs for the program and a want register.
static void decider_inputs(const Op *ops, uint32_t want, int in[16]) {
    for (int i = 0; i < MAX_STEPS; i++) {
        in[2 * i] = (int)ops[i] / 2;
        in[2 * i + 1] = (int)ops[i] % 2;
    }
    for (int n = 0; n < NUM_INPUTS; n++) in[12 + n] = (int)(want >> n & 1);
}

static int wire_named(const char *part, const char *name) {
    for (int i = 0; i < NET.num_wires; i++) {
        if (strcmp(NET.wires[i].part, part) == 0 && strcmp(NET.wires[i].name, name) == 0) return i;
    }
    stop("the decider has no such wire");
    return -1;
}

// The decider of "The circuit, in full", its program register holding 1689
// and its want register {0, 2, 3}: each core's answer, its comparator, and
// the one wire that says "solved".
static void decider_box(void) {
    circuit_decides(&NET);
    int in[16];
    decider_inputs(GUIDE, GUIDE_LANGUAGE, in);
    net_eval(&NET, in, VALUES);
    printf("```text 1689, here: the decider, want register 1011, program register ");
    machine_code(GUIDE);
    printf("\ncore  accepted  want  same?\n");
    for (int n = 0; n < NUM_INPUTS; n++) {
        char core[CIRCUIT_NAME], same[CIRCUIT_NAME];
        snprintf(core, sizeof core, "core %d step 6", n);
        snprintf(same, sizeof same, "same %d", n);
        int a = VALUES[net_port_wire(&NET, core, "accepted'")];
        printf("%d     %d         %d     %s\n", n, a, in[12 + n], VALUES[wire_named(same, "same")] ? "yes" : "no");
    }
    printf("decides = %d\n```\n\n", VALUES[net_port_wire(&NET, "", "decides")]);
}

// The same decider run backwards: the want register fixed, `decides`
// required, and every setting of the program's twelve wires tried.
static void backwards_box(void) {
    circuit_decides(&NET);
    int decides = net_port_wire(&NET, "", "decides");
    int lit = 0;
    Op found[MAX_STEPS] = {OP_NADA};
    for (int code = 0; code < NUM_PROGRAMS; code++) {
        Op ops[MAX_STEPS];
        for (int i = 0; i < MAX_STEPS; i++) ops[i] = (Op)(code >> (2 * (MAX_STEPS - 1 - i)) & 3);
        int in[16];
        decider_inputs(ops, GUIDE_LANGUAGE, in);
        net_eval(&NET, in, VALUES);
        if (VALUES[decides]) {
            lit++;
            memcpy(found, ops, sizeof found);
        }
    }
    if (lit != 1 || book_number(found) != GUIDE_NUMBER) stop("the decider lights for more than program 1689");
    printf("```text 1689, here: the circuit turned around, want 1011 fixed and decides = 1 required\n");
    printf("settings of v1 .. v12 tried: %d\n", NUM_PROGRAMS);
    printf("settings that light decides: %d\n", lit);
    machine_code(found);
    printf(": ");
    op_list(found);
    printf(", program %d\n```\n\n", book_number(found));
}

// SOLVABLE's certificate for ({0, 2, 3}, 6), and DECIDES's check of it.
static void certificate_box(void) {
    printf("```text 1689, here: the certificate that ({0, 2, 3}, 6) is in SOLVABLE\n");
    printf("(W, k) as one string:  ");
    for (int n = 0; n < NUM_INPUTS; n++) printf("%d", GUIDE_LANGUAGE >> n & 1);
    printf(" ");
    for (int k = 0; k < MAX_STEPS; k++) printf("1");
    printf("\nthe certificate P:     ");
    machine_code(GUIDE);
    printf("\n(P, W) as one string:  ");
    machine_code(GUIDE);
    printf(" ");
    for (int n = 0; n < NUM_INPUTS; n++) printf("%d", GUIDE_LANGUAGE >> n & 1);
    uint32_t decided = encoded_language(get_language_that_program_accepts(guide()));
    printf("\nthe verifier runs P on 0 to 3: it decides ");
    print_set(decided);
    printf(", so (P, W) is in DECIDES: %s\n", decided == GUIDE_LANGUAGE ? "yes" : "no");
    // A false certificate, which the verifier refuses: is_huge's program.
    static const Op FALSE_P[MAX_STEPS] = {OP_DECR, OP_DECR, OP_DECR, OP_ZERO, OP_NADA, OP_NADA};
    Program q;
    for (int i = 0; i < MAX_STEPS; i++) q.ops[i] = FALSE_P[i];
    uint32_t wrong = encoded_language(get_language_that_program_accepts(q));
    if (wrong == GUIDE_LANGUAGE) stop("the false certificate decides {0, 2, 3}");
    printf("a false certificate, ");
    machine_code(FALSE_P);
    printf(" (program %d): it decides ", book_number(FALSE_P));
    print_set(wrong);
    printf(", so the verifier refuses it\n```\n\n");
}

// The step polynomials of stepper.c, evaluated once a line, as Steve's
// test_with_stepper does: `accepted` after each line, on each input.
static void stepper_box(void) {
    printf("```text 1689, here: the step polynomials, evaluated once a line\n");
    printf("input  accepted after line 1 2 3 4 5 6  test_with_stepper\n");
    for (int n = 0; n < NUM_INPUTS; n++) {
        StepState s = {n, 0, 0, 0};
        printf("%d                          ", n);
        for (int k = 0; k < MAX_STEPS; k++) {
            s = step(s.ax, s.halted, s.accepted, GUIDE[k]);
            printf(" %d", s.accepted != 0);
        }
        int says = test_with_stepper(guide(), n);
        if (says != (run_progam(n, guide()) == STATUS_ACCEPTED)) stop("the stepper differs from the machine");
        printf("  %s\n", says ? "accepts" : "rejects");
    }
    printf("```\n\n");
}

// The composed `accepted` evaluated at input 0 by hand: the variables that
// are 1 there, then the terms whose variables are all 1, counted and summed
// by degree, and their total, the machine's verdict.
static void worked_box(const BPoly *accepted) {
    enum { INPUT = 0 };
    BVars vars = compose_vars();
    uint32_t ones = compose_ones(INPUT, guide());
    printf("```text 1689, here: accepted at input %d, worked term by term\n", INPUT);
    printf("the variables at 1:");
    for (int v = 0; v < COMPOSE_VARS; v++) {
        if (ones >> v & 1) printf(" %s", vars.names[v]);
    }
    int count[COMPOSE_VARS + 1] = {0};
    int64_t sum[COMPOSE_VARS + 1] = {0};
    int live = 0;
    for (size_t t = 0; t < accepted->len; t++) {
        uint32_t mask = accepted->terms[t].mask;
        if ((mask & ~ones) != 0) continue;
        int degree = __builtin_popcount(mask);
        count[degree]++;
        sum[degree] += accepted->terms[t].coeff;
        live++;
    }
    printf("\nterms with every variable at 1: %d of the %zu,%03zu\n", live, accepted->len / 1000, accepted->len % 1000);
    printf("degree  terms  their sum\n");
    int64_t total = 0;
    for (int d = 0; d <= COMPOSE_VARS; d++) {
        if (count[d] == 0) continue;
        printf("%-7d %-6d %+lld\n", d, count[d], (long long)sum[d]);
        total += sum[d];
    }
    printf("accepted =");
    int first = 1;
    for (int d = 0; d <= COMPOSE_VARS; d++) {
        if (count[d] == 0) continue;
        if (first) printf(" %lld", (long long)sum[d]);
        else printf(" %s %lld", sum[d] < 0 ? "-" : "+", (long long)(sum[d] < 0 ? -sum[d] : sum[d]));
        first = 0;
    }
    printf(" = %lld\n```\n\n", (long long)total);
    if (total != (run_progam(INPUT, guide()) == STATUS_ACCEPTED)) stop("the worked sum differs from the machine");
}

// The four composed polynomials at the program's four points, and the terms
// of `accepted` that are 1 there: a multilinear polynomial's value at a 0/1
// point is the sum of the coefficients of its terms whose variables are all
// 1 at that point.
static void polynomial_boxes(void) {
    BState s = compose_start();
    for (int i = 1; i <= MAX_STEPS; i++) {
        BState next = compose_step(&s, i);
        bstate_free(&s);
        s = next;
    }
    printf("```text 1689, here: the four composed polynomials at its four points\n");
    printf("input  hb  lb  halted  accepted\n");
    for (int n = 0; n < NUM_INPUTS; n++) {
        uint32_t ones = compose_ones(n, guide());
        int64_t a = bp_eval(&s.accepted, ones);
        if (a != (run_progam(n, guide()) == STATUS_ACCEPTED)) stop("the composed accepted differs from the machine");
        printf("%d      %lld   %lld   %lld       %lld\n", n, (long long)bp_eval(&s.hb, ones), (long long)bp_eval(&s.lb, ones),
               (long long)bp_eval(&s.halted, ones), (long long)a);
    }
    printf("```\n\n");
    printf("```text 1689, here: the terms of accepted that are 1 at its four points\n");
    printf("input  terms of the %zu,%03zu that are 1 there  their coefficients' sum\n", s.accepted.len / 1000, s.accepted.len % 1000);
    for (int n = 0; n < NUM_INPUTS; n++) {
        uint32_t ones = compose_ones(n, guide());
        int live = 0;
        int64_t sum = 0;
        for (size_t t = 0; t < s.accepted.len; t++) {
            if ((s.accepted.terms[t].mask & ~ones) == 0) {
                live++;
                sum += s.accepted.terms[t].coeff;
            }
        }
        printf("%d      %-37d  %lld\n", n, live, (long long)sum);
    }
    printf("```\n\n");
    worked_box(&s.accepted);
    bstate_free(&s);
}

// Cook's tableau of the program: each cell a core's four state wires after
// that row's line, `hb lb · halted accepted`, from the stepper's compiled
// step.
static void tableau_box(void) {
    printf("```text 1689, here: the tableau of program 1689\n");
    printf("step   line  core 0     core 1     core 2     core 3\n");
    StepState s[NUM_INPUTS];
    for (int n = 0; n < NUM_INPUTS; n++) s[n] = (StepState){n, 0, 0, 0};
    for (int k = 0; k <= MAX_STEPS; k++) {
        if (k == 0) printf("start       ");
        else {
            printf("%-6d %-5s", k, op_name(GUIDE[k - 1]));
            for (int n = 0; n < NUM_INPUTS; n++) s[n] = step_compiled(s[n].ax, s[n].halted, s[n].accepted, GUIDE[k - 1]);
        }
        for (int n = 0; n < NUM_INPUTS; n++) {
            printf(" %d %d · %d %d%s", s[n].ax / 2, s[n].ax % 2, s[n].halted != 0, s[n].accepted != 0, n < 3 ? " " : "");
        }
        printf("\n");
    }
    printf("```\n\n");
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) stop("cannot read a file of sat/proofs/");
    static char buf[1 << 20];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    if (n == sizeof buf - 1) stop("a file of sat/proofs/ is past the buffer");
    buf[n] = '\0';
    return buf;
}

// The CNF of {0, 2, 3}, rocsat's committed answer read as a program, and the
// size of its committed proof that five lines are too few.
static void sat_box(void) {
    SatPuzzle p = sat_puzzle(GUIDE_LANGUAGE);
    Cnf c = sat_encode(&p);
    printf("```text 1689, here: the CNF of {0, 2, 3}, and rocsat's answers\n");
    printf("sat %d: %d variables, %zu clauses\n", GUIDE_LANGUAGE, c.num_vars, c.num_clauses);
    sat_free(&c);
    const char *model = strstr(read_file("sat/proofs/13-len6.model"), "\nv ");
    if (model == NULL) stop("sat/proofs/13-len6.model holds no v line");
    int lits[SAT_PROGRAM_VARS];
    const char *at = model + 3;
    for (int i = 0; i < SAT_PROGRAM_VARS; i++) {
        char *end;
        lits[i] = (int)strtol(at, &end, 10);
        if (end == at || abs(lits[i]) != i + 1) stop("the model's v line does not open with variables 1 to 12");
        at = end;
    }
    printf("rocsat's model, v1 to v12:");
    Op ops[MAX_STEPS];
    for (int i = 0; i < SAT_PROGRAM_VARS; i++) printf(" %d", lits[i]);
    for (int i = 0; i < MAX_STEPS; i++) ops[i] = (Op)(2 * (lits[2 * i] > 0) + (lits[2 * i + 1] > 0));
    printf("\nread two bits a line:     ");
    machine_code(ops);
    printf(", program %d\n", book_number(ops));
    if (book_number(ops) != GUIDE_NUMBER) stop("rocsat's model is another program");
    const char *proof = read_file("sat/proofs/13-len5.drat");
    int lemmas = 0;
    for (const char *line = proof; *line;) {
        const char *next = strchr(line, '\n');
        if (next == NULL) next = line + strlen(line);
        if (next > line && !(line[0] == 'd' && line[1] == ' ')) lemmas++;
        line = *next ? next + 1 : next;
    }
    printf("sat %d len=5: UNSATISFIABLE, its DRAT proof %d lemmas\n```\n\n", GUIDE_LANGUAGE, lemmas);
}

int main(void) {
    if (book_number(GUIDE) != GUIDE_NUMBER) stop("the program is not program 1689");
    if (encoded_language(get_language_that_program_accepts(guide())) != GUIDE_LANGUAGE) stop("program 1689 decides another problem");
    run_box();
    sandbox_box();
    gates_box();
    formula_box();
    decider_box();
    certificate_box();
    stepper_box();
    polynomial_boxes();
    backwards_box();
    tableau_box();
    sat_box();
    return fflush(stdout) == 0 ? 0 : 1;
}
