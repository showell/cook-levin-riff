// test_theory_plus.c: the theorems of docs/NIBBLE_PLUS_THEORY.md held to
// the census (src/census.c), which shares no code with them, on all 65,536
// formal languages. The recursion (src/theory_plus.c) gives every formal
// language's shortest length under each of the four opcode sets, and its
// program decides the formal language on the plain interpreter at that
// length; the {zero, decr, mod2} closed form agrees with the census; and
// the {zero, decr, mod2} counts by length are the tiling counts the proof
// derives, with the two formal languages lost at length 16 named. For
// `half`: exactly when it helps over {zero, decr}, a conjecture that it
// helps alike beside `mod2`, the refuted claim that four ops are never
// better than the better three, and the recursion's bound on splits.

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
    // Length 4: three walks, two parity forms and all 16.
    const uint32_t four[6] = {0x5u, 0x6u, 0x8u, 0xAAABu, 0x5554u, 0xFFFFu};
    long at_four = 0;
    for (uint32_t y = 0; y < PLUS_LANGUAGES; y++) at_four += censuses[i].shortest[y] == 4;
    EXPECT_EQ(at_four, 6);
    for (int k = 0; k < 6; k++) EXPECT_EQ(censuses[i].shortest[four[k]], 4);
    const uint32_t fourteen = 1u << 14;
    const uint32_t twelve_fourteen = (1u << 12) | (1u << 14);
    EXPECT_EQ(theory_walk_length(fourteen), 15);
    EXPECT_EQ(censuses[i].shortest[fourteen], 15);
    EXPECT_EQ(theory_walk_length(twelve_fourteen), 16);
    EXPECT_EQ(censuses[i].shortest[twelve_fourteen], 14);
}

// Half's blocks from d on 16 inputs: {d, d + 1}, {d + 2, d + 3}, ..., the
// last {15} alone when 15 - d is even. Gives 1 when S from d on is a union
// of whole blocks, input d alone allowed (a `zero` takes it before the
// `half`), and says whether some accepted block is a pair and whether any
// block is accepted at all.
static int lines_up_for_half(uint32_t s, int d, int *a_pair, int *any) {
    *a_pair = 0;
    *any = 0;
    for (int b = d; b < PLUS_INPUTS; b += 2) {
        uint32_t block = 1u << b;
        if (b + 1 < PLUS_INPUTS) block |= 1u << (b + 1);
        uint32_t in = s & block;
        if (in == 0) continue;
        if (in == block) {
            *any = 1;
            if (b + 1 < PLUS_INPUTS) *a_pair = 1;
            continue;
        }
        if (b == d && in == 1u << d) continue;
        return 0;
    }
    return 1;
}

// For {zero, decr, half}: `half` shortens a formal language's program
// exactly when, for some d, S from d on is a union of half's blocks with an
// accepted pair, or S holds 15 and nothing else above 11. It helps 38,911
// formal languages. 13,654 hold 15 and line up only with {15} accepted, and
// `half` helps 3,414 of them, those with nothing from 12 to 14.
TEST(half_helps_where_its_blocks_line_up) {
    int zd = set_index(OPS_ZD);
    int zdh = set_index(OPS_ZDH);
    long helped = 0;
    long between = 0;
    long between_helped = 0;
    for (uint32_t y = 0; y < PLUS_LANGUAGES; y++) {
        int helps = censuses[zdh].shortest[y] < censuses[zd].shortest[y];
        int some_block = 0;
        int some_pair = 0;
        for (int d = 0; d < PLUS_INPUTS; d++) {
            int a_pair;
            int any;
            if (!lines_up_for_half(y, d, &a_pair, &any)) continue;
            if (any) some_block = 1;
            if (a_pair) some_pair = 1;
        }
        int above_eleven = 0;
        for (int i = 12; i < 15; i++) above_eleven |= (int)((y >> i) & 1u);
        int fifteen_alone_high = ((y >> 15) & 1u) && !above_eleven;
        if (helps != (some_pair || fifteen_alone_high)) fprintf(stderr, "  language %u: helps %d\n", y, helps);
        EXPECT_EQ(helps, some_pair || fifteen_alone_high);
        if (helps) EXPECT(some_block);
        if (some_block && !some_pair) {
            between++;
            if (helps) between_helped++;
        }
        helped += helps;
    }
    EXPECT_EQ(helped, 38911);
    // Neighbours are not needed: 377 helped have no two neighbouring
    // members, {15} first, 8 lines against its walk's 16.
    long no_neighbours = 0;
    for (uint32_t y = 0; y < PLUS_LANGUAGES; y++) {
        if (censuses[zdh].shortest[y] < censuses[zd].shortest[y] && (y & (y >> 1)) == 0) no_neighbours++;
    }
    EXPECT_EQ(no_neighbours, 377);
    EXPECT_EQ(censuses[zdh].shortest[1u << 15], 8);
    EXPECT_EQ(censuses[zd].shortest[1u << 15], 16);
    EXPECT_EQ(between, 13654);
    EXPECT_EQ(between_helped, 3414);
}

// Conjecture, checked here only: `half` helps a formal language over
// {zero, decr} exactly when it helps it over {zero, decr, mod2}. And
// refuted: the four-op length is not always the better of the two
// three-op lengths; 4,178 formal languages need `mod2` and `half` both,
// the inputs whose bit 2 is clear among them.
TEST(half_helps_alike_with_and_without_mod2) {
    int zd = set_index(OPS_ZD);
    int zdm = set_index(OPS_ZDM);
    int zdh = set_index(OPS_ZDH);
    int all = set_index(OPS_ALL);
    long both_needed = 0;
    for (uint32_t y = 0; y < PLUS_LANGUAGES; y++) {
        int over_walk = censuses[zdh].shortest[y] < censuses[zd].shortest[y];
        int over_parity = censuses[all].shortest[y] < censuses[zdm].shortest[y];
        EXPECT_EQ(over_walk, over_parity);
        int better = censuses[zdm].shortest[y] < censuses[zdh].shortest[y] ? censuses[zdm].shortest[y] : censuses[zdh].shortest[y];
        EXPECT(censuses[all].shortest[y] <= better);
        if (censuses[all].shortest[y] < better) both_needed++;
    }
    EXPECT_EQ(both_needed, 4178);
    // Bit 2 of the input clear: `half, half, mod2, zero`.
    const uint32_t bit_two_clear = 0x0F0Fu;
    EXPECT_EQ(censuses[zdm].shortest[bit_two_clear], 19);
    EXPECT_EQ(censuses[zdh].shortest[bit_two_clear], 6);
    EXPECT_EQ(censuses[all].shortest[bit_two_clear], 4);
}

// The recursion's programs: a `mod2` or `half` always maps onto a smaller
// machine (16, then at most 8, 4, 2 and 1 inputs; `mod2` to at most 2), so
// they hold at most four such lines, at most one `mod2`, and after a `mod2`
// at most one `half`.
TEST(the_recursions_programs_split_at_most_four_times) {
    for (int k = 0; k < NUM_SETS; k++) {
        int i = set_index(SETS[k]);
        for (uint32_t y = 0; y < PLUS_LANGUAGES; y++) {
            PlusOp ops[CENSUS_MAX_LENGTH];
            int len = theory_witness(&theories[i], y, ops);
            int splits = 0;
            int mod2s = 0;
            int halves_after_mod2 = 0;
            for (int j = 0; j < len; j++) {
                if (ops[j] == PLUS_MOD2) mod2s++;
                if (ops[j] == PLUS_HALF && mod2s > 0) halves_after_mod2++;
                if (ops[j] == PLUS_MOD2 || ops[j] == PLUS_HALF) splits++;
            }
            EXPECT(splits <= 4);
            EXPECT(mod2s <= 1);
            EXPECT(halves_after_mod2 <= 1);
        }
    }
}

TEST_MAIN(TEST_CASE(the_recursion_gives_every_census_length_for_every_opcode_set),
          TEST_CASE(the_recursions_programs_decide_their_formal_languages),
          TEST_CASE(the_zero_decr_mod2_closed_form_is_the_census),
          TEST_CASE(the_zero_decr_mod2_counts_are_the_tiling_counts),
          TEST_CASE(half_helps_where_its_blocks_line_up),
          TEST_CASE(half_helps_alike_with_and_without_mod2),
          TEST_CASE(the_recursions_programs_split_at_most_four_times))
