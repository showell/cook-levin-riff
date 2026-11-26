// test_theorems.c: the book's theorems about the toy, proved by running all
// 4,096 programs on all four inputs: each language's shortest length and
// the final zero of its shortest programs, Steve's analytical solution on
// every language, which opcodes are needed, and what instructions of any
// kind can do, one and two of them. A program's
// length is its instructions before its trailing `nada`s. The counts
// themselves are tests/golden/find_solutions.txt's, held by test_golden.c.

#include "test.h"

#include "../src/vm.c"
#include "../src/boolean.c"

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

// The languages decided by the six-line programs whose every op is in
// `ops`, a set of op codes as bits (bit k for op k), as a count.
static int languages_decided_by(int ops) {
    int found[NUM_LANGUAGES] = {0};
    for (int n = 0; n < NUM_PROGRAMS; n++) {
        Program p = disassemble((uint32_t)n);
        int inside = 1;
        for (int i = 0; i < MAX_STEPS; i++) inside &= (ops >> p.ops[i]) & 1;
        if (inside) found[language_code(p)] = 1;
    }
    int languages = 0;
    for (int y = 0; y < NUM_LANGUAGES; y++) languages += found[y];
    return languages;
}

#define BIT(op) (1 << (op))

// zero, decr and mod2 decide all 16 with six lines; drop any one and fewer
// are decided: without zero only { }, without decr 3, without mod2 15.
// nada is redundant: the programs without it still decide all 16.
TEST(zero_decr_and_mod2_are_each_needed_and_together_suffice) {
    int all = BIT(OP_NADA) | BIT(OP_ZERO) | BIT(OP_DECR) | BIT(OP_MOD2);
    EXPECT_EQ(languages_decided_by(all), 16);
    EXPECT_EQ(languages_decided_by(all & ~BIT(OP_NADA)), 16);
    EXPECT_EQ(languages_decided_by(all & ~BIT(OP_ZERO)), 1);
    EXPECT_EQ(languages_decided_by(all & ~BIT(OP_DECR)), 3);
    EXPECT_EQ(languages_decided_by(all & ~BIT(OP_MOD2)), 15);
}

// An instruction of any kind: what it does at each AX, a new AX (0 to 3),
// or ACCEPT or REJECT, which halt the run. 6^4 = 1,296 of them.
enum { ACCEPT = 4, REJECT = 5, NUM_ACTIONS = 6, NUM_INSTRUCTIONS = 1296 };

typedef struct {
    int at[NUM_INPUTS];
} Instruction;

static Instruction instruction(int code) {
    Instruction f;
    for (int a = 0; a < NUM_INPUTS; a++) {
        f.at[a] = code % NUM_ACTIONS;
        code /= NUM_ACTIONS;
    }
    return f;
}

// The language `count` instructions decide: the inputs they accept, a run
// that passes the last one rejecting.
static int language_of_instructions(const Instruction *fs, int count) {
    int code = 0;
    for (int n = 0; n < NUM_INPUTS; n++) {
        int ax = n;
        for (int k = 0; k < count && ax < NUM_INPUTS; k++) ax = fs[k].at[ax];
        if (ax == ACCEPT) code |= 1 << n;
    }
    return code;
}

// Two instructions decide all 16: decr, and its twin that accepts at 0.
// The program for a language is four lines, line i + 1 the twin when input
// i is in the language and decr otherwise: input n reaches 0 on line n + 1,
// where that line's verdict is the language's.
TEST(decr_and_its_accepting_twin_decide_every_language_in_four_lines) {
    const Instruction decr = {{REJECT, 0, 1, 2}};
    const Instruction twin = {{ACCEPT, 0, 1, 2}};
    for (int y = 0; y < NUM_LANGUAGES; y++) {
        Instruction fs[NUM_INPUTS];
        for (int i = 0; i < NUM_INPUTS; i++) fs[i] = (y >> i) & 1 ? twin : decr;
        EXPECT_EQ(language_of_instructions(fs, NUM_INPUTS), y);
    }
}

// One instruction cannot: for each of the 1,296, the programs of every
// length from 0 to 1,296 that repeat it decide at most 5 languages, and
// some instruction decides 5. (The four inputs' joint state takes at most
// 6^4 values, so a longer program repeats an earlier state. An accept is
// for good, so each length's language holds the one before it: 5 is the
// longest such chain of subsets of four inputs.)
TEST(one_instruction_decides_at_most_five_languages_at_any_length) {
    int most = 0;
    for (int code = 0; code < NUM_INSTRUCTIONS; code++) {
        Instruction f = instruction(code);
        int found[NUM_LANGUAGES] = {0};
        int at[NUM_INPUTS];
        for (int n = 0; n < NUM_INPUTS; n++) at[n] = n;
        for (int len = 0; len <= NUM_INSTRUCTIONS; len++) {
            int y = 0;
            for (int n = 0; n < NUM_INPUTS; n++) {
                if (at[n] == ACCEPT) y |= 1 << n;
            }
            found[y] = 1;
            for (int n = 0; n < NUM_INPUTS; n++) {
                if (at[n] < NUM_INPUTS) at[n] = f.at[at[n]];
            }
        }
        int languages = 0;
        for (int y = 0; y < NUM_LANGUAGES; y++) languages += found[y];
        if (languages > most) most = languages;
    }
    EXPECT_EQ(most, 5);
}

// The 16 languages are exactly the 16 Boolean functions of hb and lb: each
// language's expression is 1 at the bits of its members alone, the 16
// expressions' truth tables are 16 different ones, and each language is the
// one some program accepts.
TEST(the_16_languages_are_the_16_boolean_functions_of_two_bits) {
    int seen[NUM_LANGUAGES] = {0};
    int decided[NUM_LANGUAGES] = {0};
    for (int n = 0; n < NUM_PROGRAMS; n++) decided[language_code(disassemble((uint32_t)n))] = 1;
    for (int y = 0; y < NUM_LANGUAGES; y++) {
        int table = language_of_expression(LANGUAGE_EXPRESSIONS[y]);
        EXPECT_EQ(table, y);
        seen[table]++;
        EXPECT(decided[y]);
    }
    for (int y = 0; y < NUM_LANGUAGES; y++) EXPECT_EQ(seen[y], 1);
    // The parser's reading: NOT binds tightest, then AND, then OR and XOR.
    EXPECT_EQ(language_of_expression("NOT hb AND lb"), 2);
    EXPECT_EQ(language_of_expression("NOT (hb AND lb)"), 7);
    EXPECT_EQ(language_of_expression("hb OR lb AND 0"), 12);
}

TEST_MAIN(TEST_CASE(each_language_has_the_shortest_length_the_book_states),
          TEST_CASE(every_shortest_program_ends_in_zero),
          TEST_CASE(the_analytical_solution_decides_every_language),
          TEST_CASE(without_mod2_fifteen_languages_are_decided),
          TEST_CASE(zero_decr_and_mod2_are_each_needed_and_together_suffice),
          TEST_CASE(decr_and_its_accepting_twin_decide_every_language_in_four_lines),
          TEST_CASE(one_instruction_decides_at_most_five_languages_at_any_length),
          TEST_CASE(the_16_languages_are_the_16_boolean_functions_of_two_bits))
