// compose.c: six steps composed in the Boolean ring (compose.h).

#include "compose.h"

#include "stepper.h"

#include <stdio.h>
#include <stdlib.h>

static const char *const NAMES[COMPOSE_VARS] = {
    "h0",     "l0",     "op1_hb", "op1_lb", "op2_hb", "op2_lb", "op3_hb",
    "op3_lb", "op4_hb", "op4_lb", "op5_hb", "op5_lb", "op6_hb", "op6_lb",
};

BVars compose_vars(void) {
    BVars v = {{0}, COMPOSE_VARS};
    for (int i = 0; i < COMPOSE_VARS; i++) v.names[i] = NAMES[i];
    return v;
}

static void check_step(int i) {
    if (i < 1 || i > MAX_STEPS) {
        fprintf(stderr, "compose: step %d is not in 1..%d\n", i, MAX_STEPS);
        abort();
    }
}

int compose_op_hb(int i) {
    check_step(i);
    return 2 * i;
}

int compose_op_lb(int i) {
    check_step(i);
    return 2 * i + 1;
}

// The four step polynomials cut to x² = x, over the six slots (bit s is
// stepper.h's slot s), made once.
static BPoly STEP[4];
static int step_made;

static const BPoly *step_bpolys(void) {
    if (!step_made) {
        BVars v = {{"hb", "lb", "halted", "accepted", "op_hb", "op_lb"}, NUM_SLOTS};
        const StepPolys *p = step_polynomials();
        STEP[0] = bp_from_poly(&p->hb, &v);
        STEP[1] = bp_from_poly(&p->lb, &v);
        STEP[2] = bp_from_poly(&p->halted, &v);
        STEP[3] = bp_from_poly(&p->accepted, &v);
        step_made = 1;
    }
    return STEP;
}

BState compose_start(void) {
    BState s = {bp_var(COMPOSE_H0), bp_var(COMPOSE_L0), bp_zero(), bp_zero()};
    return s;
}

BState compose_step(const BState *before, int i) {
    const BPoly *step = step_bpolys();
    BPoly vals[NUM_SLOTS];
    vals[SLOT_HB] = before->hb;
    vals[SLOT_LB] = before->lb;
    vals[SLOT_HALTED] = before->halted;
    vals[SLOT_ACCEPTED] = before->accepted;
    BPoly op_hb = bp_var(compose_op_hb(i));
    BPoly op_lb = bp_var(compose_op_lb(i));
    vals[SLOT_OP_HB] = op_hb;
    vals[SLOT_OP_LB] = op_lb;
    BState after = {
        bp_substitute(&step[0], vals, NUM_SLOTS),
        bp_substitute(&step[1], vals, NUM_SLOTS),
        bp_substitute(&step[2], vals, NUM_SLOTS),
        bp_substitute(&step[3], vals, NUM_SLOTS),
    };
    bp_free(&op_hb);
    bp_free(&op_lb);
    return after;
}

void bstate_free(BState *s) {
    bp_free(&s->hb);
    bp_free(&s->lb);
    bp_free(&s->halted);
    bp_free(&s->accepted);
}

uint32_t compose_ones(int input, Program program) {
    if (input < 0 || input > 3) {
        fprintf(stderr, "compose: input %d is not in 0..3\n", input);
        abort();
    }
    uint32_t ones = ((uint32_t)(input / 2) << COMPOSE_H0) | ((uint32_t)(input % 2) << COMPOSE_L0);
    for (int i = 1; i <= MAX_STEPS; i++) {
        int op = (int)program.ops[i - 1];
        ones |= ((uint32_t)(op / 2) << compose_op_hb(i)) | ((uint32_t)(op % 2) << compose_op_lb(i));
    }
    return ones;
}

int compose_input_of(uint32_t ones) {
    return 2 * (int)((ones >> COMPOSE_H0) & 1) + (int)((ones >> COMPOSE_L0) & 1);
}

Program compose_program_of(uint32_t ones) {
    Program program;
    for (int i = 1; i <= MAX_STEPS; i++) {
        int op = 2 * (int)((ones >> compose_op_hb(i)) & 1) + (int)((ones >> compose_op_lb(i)) & 1);
        program.ops[i - 1] = (Op)op;
    }
    return program;
}
