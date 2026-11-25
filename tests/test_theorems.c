// test_theorems.c: the book's theorems about the toy, proved by running all
// 4,096 programs on all four inputs: each language's shortest length and
// the final zero of its shortest programs, Steve's analytical solution on
// every language, and the languages decided without mod2. A program's
// length is its instructions before its trailing `nada`s. The counts
// themselves are tests/golden/find_solutions.txt's, held by test_golden.c.

#include "test.h"

#include "../src/vm.c"

static int length_of(Program p) {
    int len = 0;
    for (int i = 0; i < MAX_STEPS; i++) {
        if (p.ops[i] != OP_NADA) len = i + 1;
    }
    return len;
}

static int language_code(Program p) {
    return (int)encoded_language(get_language_that_program_accepts(p));
}

// Each language's shortest length, by code: the book's table.
static const int SHORTEST[NUM_LANGUAGES] = {0, 1, 2, 3, 3, 2, 4, 5, 4, 5, 3, 4, 5, 6, 5, 4};

TEST(each_language_has_the_shortest_length_the_book_states) {
    int shortest[NUM_LANGUAGES];
    for (int y = 0; y < NUM_LANGUAGES; y++) shortest[y] = MAX_STEPS + 1;
    for (int n = 0; n < NUM_PROGRAMS; n++) {
        Program p = disassemble((uint32_t)n);
        int y = language_code(p);
        if (length_of(p) < shortest[y]) shortest[y] = length_of(p);
    }
    for (int y = 0; y < NUM_LANGUAGES; y++) {
        if (shortest[y] != SHORTEST[y]) fprintf(stderr, "  language %d: shortest %d\n", y, shortest[y]);
        EXPECT_EQ(shortest[y], SHORTEST[y]);
    }
}

// Every shortest program of a line or more ends in `zero`; a program whose
// last instruction is another, `decr` or `mod2`, is counted, so the check is
// seen to find them among the longer programs.
TEST(every_shortest_program_ends_in_zero) {
    int shortest_programs = 0;
    int ends_in_zero = 0;
    int longer_ending_otherwise = 0;
    for (int n = 0; n < NUM_PROGRAMS; n++) {
        Program p = disassemble((uint32_t)n);
        int len = length_of(p);
        if (len == 0) continue;
        if (len == SHORTEST[language_code(p)]) {
            shortest_programs++;
            if (p.ops[len - 1] == OP_ZERO) ends_in_zero++;
        } else if (p.ops[len - 1] != OP_ZERO) {
            longer_ending_otherwise++;
        }
    }
    EXPECT(shortest_programs > 0);
    EXPECT_EQ(ends_in_zero, shortest_programs);
    EXPECT(longer_ending_otherwise > 0);
}

// Steve's analytical solution (virtual-machine's d6bb78f, `solve`): decr
// down to the language's least member, zero, and the same for the rest.
// Writes the ops to `ops` and gives their count, which may pass MAX_STEPS.
static int solve(int code, Op *ops) {
    int lang[NUM_INPUTS];
    int len = 0;
    for (int i = 0; i < NUM_INPUTS; i++) {
        if (code & (1 << i)) lang[len++] = i;
    }
    int n = 0;
    int first = 0;
    while (first < len) {
        while (lang[first] > 0) {
            ops[n++] = OP_DECR;
            for (int k = first; k < len; k++) lang[k]--;
        }
        ops[n++] = OP_ZERO;
        first++;
    }
    return n;
}

// The language that `count` ops accept, run by vm_step with no bound on
// their number.
static int language_of_ops(const Op *ops, int count) {
    int code = 0;
    for (int i = 0; i < NUM_INPUTS; i++) {
        Machine m = vm_start(i);
        for (int k = 0; k < count; k++) m = vm_step(m, ops[k]);
        if (m.status == STATUS_ACCEPTED) code |= 1 << i;
    }
    return code;
}

TEST(the_analytical_solution_decides_every_language) {
    Op ops[2 * NUM_INPUTS];
    for (int y = 0; y < NUM_LANGUAGES; y++) {
        int n = solve(y, ops);
        EXPECT_EQ(language_of_ops(ops, n), y);
        // Six lines or fewer for every language but {0, 1, 2, 3}, which
        // takes seven: zero, decr, zero, decr, zero, decr, zero.
        EXPECT_EQ(n > MAX_STEPS, y == 15);
        if (n <= MAX_STEPS) {
            Program p = disassemble(assemble(ops, (size_t)n));
            EXPECT_EQ(language_code(p), y);
        }
    }
    EXPECT_EQ(solve(15, ops), 7);
    // For {0, 2, 3} it builds the one program that decides it.
    const Op unique[MAX_STEPS] = {OP_ZERO, OP_DECR, OP_DECR, OP_ZERO, OP_DECR, OP_ZERO};
    EXPECT_EQ(solve(13, ops), MAX_STEPS);
    EXPECT_EQ(assemble(ops, MAX_STEPS), assemble(unique, MAX_STEPS));
}

// The 3^6 = 729 programs over nada, zero and decr decide 15 languages: all
// but {0, 1, 2, 3}.
TEST(without_mod2_fifteen_languages_are_decided) {
    int found[NUM_LANGUAGES] = {0};
    int programs = 0;
    for (int n = 0; n < NUM_PROGRAMS; n++) {
        Program p = disassemble((uint32_t)n);
        int has_mod2 = 0;
        for (int i = 0; i < MAX_STEPS; i++) has_mod2 |= p.ops[i] == OP_MOD2;
        if (has_mod2) continue;
        programs++;
        found[language_code(p)] = 1;
    }
    EXPECT_EQ(programs, 729);
    int languages = 0;
    for (int y = 0; y < NUM_LANGUAGES; y++) languages += found[y];
    EXPECT_EQ(languages, 15);
    EXPECT_EQ(found[15], 0);
    EXPECT_EQ(found[13], 1);
}

TEST_MAIN(TEST_CASE(each_language_has_the_shortest_length_the_book_states),
          TEST_CASE(every_shortest_program_ends_in_zero),
          TEST_CASE(the_analytical_solution_decides_every_language),
          TEST_CASE(without_mod2_fifteen_languages_are_decided))
