// test_boolean.c: the Boolean expressions of "Six expressions, composed",
// held to the machine: the step's expressions to the stepper's step on all
// 64 settings of its six bits, and each language's sum of products over the
// program's twelve bits to the programs run_progam says decide it.

#include "test.h"

#include "../src/vm.c"
#include "../src/stepper.c"
#include "../src/poly.c"
#include "../src/ring_int.c"
#include "../src/boolean.c"
#include "../src/decides.c"

TEST(the_steps_expressions_are_the_stepper_on_all_64_settings) {
    int differ = 0;
    int changed = 0;
    for (int bits = 0; bits < 64; bits++) {
        int hb = bits & 1, lb = (bits >> 1) & 1, halted = (bits >> 2) & 1;
        int accepted = (bits >> 3) & 1, op_hb = (bits >> 4) & 1, op_lb = (bits >> 5) & 1;
        StepState s = step_compiled(2 * hb + lb, halted, accepted, (Op)(2 * op_hb + op_lb));
        int want = 8 * (s.ax >> 1) + 4 * (s.ax & 1) + 2 * (s.halted != 0) + (s.accepted != 0);
        int got = step_by_expressions(hb, lb, halted, accepted, op_hb, op_lb);
        differ += got != want;
        changed += got != 8 * hb + 4 * lb + 2 * halted + accepted;
    }
    EXPECT_EQ(differ, 0);
    // The facts about AX are the languages {0}, {2} and {3} of hb and lb.
    EXPECT_EQ(language_of_expression(STEP_EXPRESSIONS[0].text), 1);
    EXPECT_EQ(language_of_expression(STEP_EXPRESSIONS[1].text), 4);
    EXPECT_EQ(language_of_expression(STEP_EXPRESSIONS[2].text), 8);
    // The step does change the state, so the check above can see a fault.
    EXPECT(changed > 0);
}

// A point is a program's twelve bits in the SAT chapter's numbering.
TEST(points_name_programs_as_the_sat_chapter_numbers_them) {
    // zero, decr, decr, zero, decr, zero is 01 10 10 01 10 01: v2, v3, v5,
    // v8, v9 and v12 set.
    int point = (1 << 1) | (1 << 2) | (1 << 4) | (1 << 7) | (1 << 8) | (1 << 11);
    const Op ops[MAX_STEPS] = {OP_ZERO, OP_DECR, OP_DECR, OP_ZERO, OP_DECR, OP_ZERO};
    EXPECT_EQ(program_of_point(point), assemble(ops, MAX_STEPS));
}

TEST(each_languages_sum_of_products_holds_exactly_its_programs) {
    static unsigned char is_true[POINTS];
    static Cover cover;
    int all = 0;
    for (int code = 0; code < NUM_LANGUAGES; code++) {
        points_deciding(code, is_true);
        cover_points(is_true, &cover);
        EXPECT(cover_is_exact(&cover, is_true));
        // The cover held to run_progam directly, point by point.
        for (int point = 0; point < POINTS; point++) {
            int held = 0;
            for (int i = 0; i < cover.count; i++) held |= (point & cover.cubes[i].care) == cover.cubes[i].value;
            Program p = disassemble(program_of_point(point));
            int decides = 1;
            for (int n = 0; n < NUM_INPUTS; n++) decides &= (run_progam(n, p) == STATUS_ACCEPTED) == ((code >> n) & 1);
            EXPECT_EQ(held, decides);
            all += held;
        }
        if (code == 13) {
            EXPECT_EQ(cover.count, 1);
            char text[32];
            cube_text(cover.cubes[0], text);
            EXPECT(strcmp(text, "01 10 10 01 10 01") == 0);
        }
    }
    EXPECT_EQ(all, NUM_PROGRAMS);
}

TEST_MAIN(TEST_CASE(the_steps_expressions_are_the_stepper_on_all_64_settings),
          TEST_CASE(points_name_programs_as_the_sat_chapter_numbers_them),
          TEST_CASE(each_languages_sum_of_products_holds_exactly_its_programs))
