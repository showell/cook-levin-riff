// test_reference.c: every program on every input, judged against a
// re-statement of each opcode's rule written as a table, not as run_progam's
// code. The table is the docstring of virtual_machine.py, row by row: what
// each op does to AX = 0, 1, 2, 3, and whether it halts, accepting or not.

#include "test.h"

#include "../src/vm.c"
#include "../src/search.c"

typedef enum { GO_ON, HALT_ACCEPT, HALT_REJECT } Effect;

typedef struct {
    int next_ax;
    Effect effect;
} Rule;

// RULES[op][ax], from the module's docstring.
static const Rule RULES[NUM_OPS][NUM_INPUTS] = {
    // 00 nada: do nothing
    {{0, GO_ON}, {1, GO_ON}, {2, GO_ON}, {3, GO_ON}},
    // 01 zero: AX = 0 halts and accepts; otherwise ignore and continue
    {{0, HALT_ACCEPT}, {1, GO_ON}, {2, GO_ON}, {3, GO_ON}},
    // 10 decr: AX = 0 halts and rejects; otherwise AX - 1
    {{0, HALT_REJECT}, {0, GO_ON}, {1, GO_ON}, {2, GO_ON}},
    // 11 mod2: 3 -> 1, 2 -> 0, 1 -> 1, 0 -> 0
    {{0, GO_ON}, {1, GO_ON}, {0, GO_ON}, {1, GO_ON}},
};

// The verdict by the table: the first halt decides; none is Python's None.
static Status reference_run(int ax, const Op ops[MAX_STEPS]) {
    for (int i = 0; i < MAX_STEPS; i++) {
        Rule r = RULES[ops[i]][ax];
        if (r.effect == HALT_ACCEPT) return STATUS_ACCEPTED;
        if (r.effect == HALT_REJECT) return STATUS_REJECTED;
        ax = r.next_ax;
    }
    return STATUS_NONE;
}

TEST(every_program_on_every_input_agrees_with_the_table) {
    int checked = 0;
    for (uint32_t n = 0; n < NUM_PROGRAMS; n++) {
        Program p = disassemble(n);
        for (int ax = 0; ax < NUM_INPUTS; ax++) {
            Status got = run_progam(ax, p);
            Status want = reference_run(ax, p.ops);
            if (got != want) fprintf(stderr, "  program %u on AX = %d: got %d, want %d\n", n, ax, got, want);
            EXPECT_EQ(got, want);
            checked++;
        }
    }
    EXPECT_EQ(checked, 16384);
}

TEST(each_status_occurs) {
    int seen[3] = {0, 0, 0};
    for (uint32_t n = 0; n < NUM_PROGRAMS; n++) {
        for (int ax = 0; ax < NUM_INPUTS; ax++) seen[run_progam(ax, disassemble(n))]++;
    }
    EXPECT(seen[STATUS_NONE] > 0);
    EXPECT(seen[STATUS_ACCEPTED] > 0);
    EXPECT(seen[STATUS_REJECTED] > 0);
    EXPECT_EQ(seen[0] + seen[1] + seen[2], 16384);
}

// The bucket sizes the dissection measured from Python's output, by code.
TEST(the_buckets_have_pythons_sizes) {
    static const int want[NUM_LANGUAGES] = {844, 681, 303, 172, 248, 883, 74, 13,
                                            63,  12,  520, 150, 13,  1,   15, 104};
    static Solutions s;
    solutions_fill(&s);
    size_t total = 0;
    for (int y = 0; y < NUM_LANGUAGES; y++) {
        EXPECT_EQ(s.count[y], want[y]);
        for (size_t i = 1; i < s.count[y]; i++) EXPECT(s.programs[y][i - 1] < s.programs[y][i]);
        total += s.count[y];
    }
    EXPECT_EQ(total, NUM_PROGRAMS);
}

TEST_MAIN(TEST_CASE(every_program_on_every_input_agrees_with_the_table), TEST_CASE(each_status_occurs),
          TEST_CASE(the_buckets_have_pythons_sizes))
