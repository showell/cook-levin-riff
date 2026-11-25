// test_compose.c: the six-step composition (compose.h) and its proof. The
// term counts after every step are the dissection's (its Python measured
// them), and the composed polynomials agree with the machine on all 2^14
// bit vectors, every program on every input:
//
// - accepted is 1 exactly when run_progam accepts;
// - hb and lb are the bits of the interpreter's final AX: AX when it
//   halted, or after the sixth op. The stepper's AX agrees with it on every
//   run: a halted stepper runs `nada`, so AX keeps its value, and after an
//   accept (which leaves the stepper running) AX is 0, which no op changes;
// - halted is the stepper's, not the interpreter's (stepper.h: an accept
//   does not set it), so it is held to the compiled stepper's halted, and
//   to its rule: rejected, or accepted with a `decr` somewhere after the
//   accepting `zero`.

#include "test.h"

#include "../src/vm.c"
#include "../src/stepper.c"
#include "../src/compose.c"
#include "../src/boolring.c"
#include "../src/poly.c"
#include "../src/ring_int.c"

static BState composed;
static size_t counts[MAX_STEPS][4];
static int made;

// All six steps, once; each step's term counts kept.
static const BState *all_six(void) {
    if (!made) {
        composed = compose_start();
        for (int i = 1; i <= MAX_STEPS; i++) {
            BState next = compose_step(&composed, i);
            bstate_free(&composed);
            composed = next;
            counts[i - 1][0] = composed.hb.len;
            counts[i - 1][1] = composed.lb.len;
            counts[i - 1][2] = composed.halted.len;
            counts[i - 1][3] = composed.accepted.len;
        }
        made = 1;
    }
    return &composed;
}

TEST(every_steps_term_counts_are_the_dissections) {
    all_six();
    // hb, lb, halted, accepted after steps 1..6 (dissection.md, section 3).
    static const size_t want[MAX_STEPS][4] = {
        {4, 7, 8, 8},         {11, 25, 30, 40},     {27, 79, 92, 162},
        {63, 241, 282, 642}, {143, 727, 878, 2568}, {319, 2185, 2732, 10324},
    };
    for (int i = 0; i < MAX_STEPS; i++) {
        for (int k = 0; k < 4; k++) EXPECT_EQ(counts[i][k], want[i][k]);
    }
}

TEST(the_final_coefficients_and_degrees) {
    const BState *s = all_six();
    EXPECT_EQ(bp_max_abs_coeff(&s->hb), 6);
    EXPECT_EQ(bp_max_abs_coeff(&s->lb), 21);
    EXPECT_EQ(bp_max_abs_coeff(&s->halted), 20);
    EXPECT_EQ(bp_max_abs_coeff(&s->accepted), 63);
    EXPECT_EQ(bp_degree(&s->hb), 9);
    EXPECT_EQ(bp_degree(&s->lb), 14);
    EXPECT_EQ(bp_degree(&s->halted), 14);
    EXPECT_EQ(bp_degree(&s->accepted), 14);
}

TEST(a_bit_vector_names_one_input_and_one_program) {
    for (uint32_t ones = 0; ones < (1u << COMPOSE_VARS); ones++) {
        Program p = compose_program_of(ones);
        EXPECT_EQ(compose_ones(compose_input_of(ones), p), ones);
    }
    // Program 9 (zero, decr, nada...) on input 2: h0 = 1, op1 = 1, op2 = 2.
    Program nine = disassemble(9);
    uint32_t want = (1u << COMPOSE_H0) | (1u << compose_op_lb(1)) | (1u << compose_op_hb(2));
    EXPECT_EQ(compose_ones(2, nine), want);
}

// The stepper's halted by its rule: rejected, or accepted with a `decr`
// after the accepting `zero` (AX is 0 from then on, so that decr halts).
static int stepper_halts(int input, Program program) {
    int ax = input;
    int accepted_at = -1;
    for (int i = 0; i < MAX_STEPS; i++) {
        Op op = program.ops[i];
        if (accepted_at >= 0) {
            if (op == OP_DECR) return 1;
            continue;
        }
        if (op == OP_ZERO && ax == 0) accepted_at = i;
        if (op == OP_DECR) {
            if (ax == 0) return 1;
            ax--;
        }
        if (op == OP_MOD2) ax %= 2;
    }
    return 0;
}

TEST(the_composed_polynomials_agree_with_the_machine_on_all_2_to_the_14) {
    const BState *s = all_six();
    size_t checked = 0;
    size_t accepted = 0;
    size_t differ = 0;
    size_t halted_unlike_interpreter = 0;
    for (uint32_t ones = 0; ones < (1u << COMPOSE_VARS); ones++) {
        int input = compose_input_of(ones);
        Program program = compose_program_of(ones);
        int ax = -1;
        Status status = run_progam_ax(input, program, &ax);
        int want_accepted = status == STATUS_ACCEPTED;
        StepState st = {input, 0, 0, 0};
        for (int i = 0; i < MAX_STEPS; i++) st = step_compiled(st.ax, st.halted, st.accepted, program.ops[i]);
        int64_t got_accepted = bp_eval(&s->accepted, ones);
        int64_t got_hb = bp_eval(&s->hb, ones);
        int64_t got_lb = bp_eval(&s->lb, ones);
        int64_t got_halted = bp_eval(&s->halted, ones);
        int ok = got_accepted == want_accepted && got_hb == ax / 2 && got_lb == ax % 2 && got_halted == st.halted &&
                 got_halted == stepper_halts(input, program);
        if (!ok) {
            if (differ < 5)
                fprintf(stderr, "  vector %u (input %d): accepted %lld/%d hb %lld lb %lld AX %d halted %lld/%d\n", ones,
                        input, (long long)got_accepted, want_accepted, (long long)got_hb, (long long)got_lb, ax,
                        (long long)got_halted, st.halted);
            differ++;
        }
        if (got_halted != (status != STATUS_NONE)) halted_unlike_interpreter++;
        accepted += (size_t)want_accepted;
        checked++;
    }
    EXPECT_EQ(differ, 0);
    EXPECT_EQ(checked, 16384);
    // test_cross_check.c's count of accepting (program, input) pairs.
    EXPECT_EQ(accepted, 5596);
    // The runs where an accept is not a halt: accepted with no decr after
    // (measured; each one is held to stepper_halts above).
    EXPECT_EQ(halted_unlike_interpreter, 2891);
}

TEST_MAIN(TEST_CASE(every_steps_term_counts_are_the_dissections), TEST_CASE(the_final_coefficients_and_degrees),
          TEST_CASE(a_bit_vector_names_one_input_and_one_program),
          TEST_CASE(the_composed_polynomials_agree_with_the_machine_on_all_2_to_the_14))
