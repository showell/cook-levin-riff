// test_theorems.c: the book's theorems about shortest programs, proved by
// running all 4,096 programs on all four inputs. A program's length is its
// instructions before its trailing `nada`s. The counts themselves are
// tests/golden/find_solutions.txt's, held by test_golden.c.

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

TEST_MAIN(TEST_CASE(each_language_has_the_shortest_length_the_book_states),
          TEST_CASE(every_shortest_program_ends_in_zero))
