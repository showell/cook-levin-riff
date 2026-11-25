// test_cross_check.c: the stepper against the interpreter on every program,
// a stronger check than Python's (find_solutions checks only the 16 example
// programs). Each of the 4,096 programs on each of the 4 inputs: the
// compiled stepper accepts exactly when `run_progam` does. Its own file, so
// build.shale's line for it is the cross-check's time.

#include "test.h"

#include "../src/vm.c"
#include "../src/stepper.c"
#include "../src/poly.c"
#include "../src/ring_int.c"

TEST(every_program_on_every_input_the_stepper_agrees_with_run_progam) {
    size_t checked = 0;
    size_t accepted = 0;
    size_t differ = 0;
    for (int n = 0; n < NUM_PROGRAMS; n++) {
        Program program = disassemble((uint32_t)n);
        for (int i = 0; i < NUM_INPUTS; i++) {
            int want = run_progam(i, program) == STATUS_ACCEPTED;
            int got = test_with_compiled_stepper(program, i);
            if (got != want) {
                if (differ < 5) fprintf(stderr, "  program %d on input %d: stepper %d, run_progam %d\n", n, i, got, want);
                differ++;
            }
            accepted += (size_t)want;
            checked++;
        }
    }
    EXPECT_EQ(differ, 0);
    EXPECT_EQ(checked, NUM_PROGRAMS * NUM_INPUTS);
    // The accepting pairs: the bucket sizes times their languages' sizes
    // (find_solutions.txt), summed.
    EXPECT_EQ(accepted, 5596);
}

TEST_MAIN(TEST_CASE(every_program_on_every_input_the_stepper_agrees_with_run_progam))
