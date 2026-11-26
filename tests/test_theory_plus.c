// test_theory_plus.c: the theorems of docs/NIBBLE_PLUS_THEORY.md held to
// the census (src/census.c), which shares no code with them, on all 65,536
// formal languages. The recursion (src/theory_plus.c) gives every formal
// language's shortest length under each of the four opcode sets, and its
// program decides the formal language on the plain interpreter at that
// length; the {zero, decr, mod2} closed form agrees with the census; and
// the {zero, decr, mod2} counts by length are the tiling counts the proof
// derives, with the two formal languages lost at length 16 named.

#include "test.h"

#include "../src/plus.c"
#include "../src/census.c"
#include "../src/theory_plus.c"

#define OPS_ZD (CENSUS_OP(PLUS_ZERO) | CENSUS_OP(PLUS_DECR))
#define OPS_ZDM (OPS_ZD | CENSUS_OP(PLUS_MOD2))
#define OPS_ZDH (OPS_ZD | CENSUS_OP(PLUS_HALF))
#define OPS_ALL (OPS_ZDM | CENSUS_OP(PLUS_HALF))

static const unsigned SETS[] = {OPS_ZD, OPS_ZDM, OPS_ZDH, OPS_ALL};
enum { NUM_SETS = 4 };

// Static: each census holds 64 Ki tables. The four censuses and theories
// are made once, by the first test that wants them.
static Census censuses[NUM_SETS];
static Theory theories[NUM_SETS];
static int made = 0;

static int set_index(unsigned ops) {
    if (!made) {
        for (int i = 0; i < NUM_SETS; i++) {
            census_run(&censuses[i], SETS[i]);
            theory_build(&theories[i], SETS[i]);
        }
        made = 1;
    }
    for (int i = 0; i < NUM_SETS; i++) {
        if (SETS[i] == ops) return i;
    }
    fprintf(stderr, "set_index: no set for ops %#x\n", ops);
    abort();
    return -1; // not reached; tcc can't see that abort doesn't return
}

TEST(the_recursion_gives_every_census_length_for_every_opcode_set) {
    long checked = 0;
    for (int k = 0; k < NUM_SETS; k++) {
        int i = set_index(SETS[k]);
        for (uint32_t y = 0; y < PLUS_LANGUAGES; y++) {
            int want = censuses[i].shortest[y];
            int got = theory_shortest(&theories[i], y);
            if (got != want) fprintf(stderr, "  ops %#x, language %u: census %d, recursion %d\n", SETS[k], y, want, got);
            EXPECT_EQ(got, want);
            checked++;
        }
    }
    EXPECT_EQ(checked, NUM_SETS * PLUS_LANGUAGES);
}

TEST(the_recursions_programs_decide_their_formal_languages) {
    for (int k = 0; k < NUM_SETS; k++) {
        int i = set_index(SETS[k]);
        for (uint32_t y = 0; y < PLUS_LANGUAGES; y++) {
            PlusOp ops[CENSUS_MAX_LENGTH];
            int len = theory_witness(&theories[i], y, ops);
            EXPECT_EQ(len, theory_shortest(&theories[i], y));
            for (int j = 0; j < len; j++) EXPECT(SETS[k] & CENSUS_OP(ops[j]));
            EXPECT_EQ(plus_language(ops, len), y);
        }
    }
}

TEST(the_zero_decr_mod2_closed_form_is_the_census) {
    int i = set_index(OPS_ZDM);
    for (uint32_t y = 0; y < PLUS_LANGUAGES; y++) {
        if (theory_zdm_closed(y) != censuses[i].shortest[y]) {
            fprintf(stderr, "  language %u: census %d, closed form %d\n", y, censuses[i].shortest[y], theory_zdm_closed(y));
        }
        EXPECT_EQ(theory_zdm_closed(y), censuses[i].shortest[y]);
    }
}

// Walks as tilings: a walk of d `decr`s with a `zero` before some of them
// is a row of tiles, `decr` (one line) and `zero, decr` (two). The number of
// rows of m lines, all of them, those ending in `zero, decr` then `decr`,
// and those that are empty or end in `decr`.
static long rows_all(int m) {
    if (m < 0) return 0;
    long a = 1;  // rows of 0 lines
    long b = 1;  // rows of 1 line
    if (m == 0) return a;
    for (int i = 2; i <= m; i++) {
        long c = a + b;
        a = b;
        b = c;
    }
    return b;
}

static long rows_ending_zero_decr_decr(int m) {
    return rows_all(m - 3);
}

static long rows_empty_or_ending_decr(int m) {
    if (m == 0) return 1;
    return rows_all(m - 1);
}

// The proof's count: at length L up to 15, the walks' F(L), the parity
// forms that no smaller d undercuts, and the all-from-d forms that no
// smaller d undercuts. At 16, all of those but two: {14}, whose parity form
// is a line longer than its walk, and {12, 14}, whose walk of 16 lines the
// parity form from 12 undercuts by two.
TEST(the_zero_decr_mod2_counts_are_the_tiling_counts) {
    int i = set_index(OPS_ZDM);
    for (int L = 1; L <= 16; L++) {
        long walks = rows_all(L - 1);  // F(L): max(S) + |S| = L
        long parity = rows_all(L - 2) - rows_ending_zero_decr_decr(L - 2);
        long all_from = L >= 4 ? rows_empty_or_ending_decr(L - 4) : 0;
        long want = walks + parity + all_from;
        if (L == 16) want -= 2;
        EXPECT_EQ(censuses[i].histogram[L], want);
    }
    // F(L + 1) at every length from 1 to 15 but 4, which has one more.
    for (int L = 1; L <= 15; L++) EXPECT_EQ(censuses[i].histogram[L], rows_all(L) + (L == 4 ? 1 : 0));
    const uint32_t fourteen = 1u << 14;
    const uint32_t twelve_fourteen = (1u << 12) | (1u << 14);
    EXPECT_EQ(theory_walk_length(fourteen), 15);
    EXPECT_EQ(censuses[i].shortest[fourteen], 15);
    EXPECT_EQ(theory_walk_length(twelve_fourteen), 16);
    EXPECT_EQ(censuses[i].shortest[twelve_fourteen], 14);
}

TEST_MAIN(TEST_CASE(the_recursion_gives_every_census_length_for_every_opcode_set),
          TEST_CASE(the_recursions_programs_decide_their_formal_languages),
          TEST_CASE(the_zero_decr_mod2_closed_form_is_the_census),
          TEST_CASE(the_zero_decr_mod2_counts_are_the_tiling_counts))
