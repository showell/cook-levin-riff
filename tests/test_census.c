// test_census.c: the Nibble++ census (src/census.c) held to claims it does
// not make itself. Every witness, for every opcode set and every one of the
// 65,536 languages, runs on the plain interpreter (src/plus.c) and must
// decide exactly its language at its census length; short programs are
// enumerated outright, so the census lengths are seen to be the shortest;
// and three theorems are checked on the whole census: `decr` is a safe pad,
// `zero` and `decr` alone take max(S) + |S| lines, and the Fibonacci
// numbers among the {zero, decr, mod2} counts. The histograms themselves are
// tests/golden/census.txt's, which build.shale holds.

#include "test.h"

#include "../src/plus.c"
#include "../src/census.c"

#define OPS_ZD (CENSUS_OP(PLUS_ZERO) | CENSUS_OP(PLUS_DECR))
#define OPS_ZDM (OPS_ZD | CENSUS_OP(PLUS_MOD2))
#define OPS_ZDH (OPS_ZD | CENSUS_OP(PLUS_HALF))
#define OPS_ALL (OPS_ZDM | CENSUS_OP(PLUS_HALF))

static const unsigned SETS[] = {OPS_ZD, OPS_ZDM, OPS_ZDH, OPS_ALL};
enum { NUM_SETS = 4 };

// Static: each census holds 64 Ki tables. The four are run once, by the
// first test that wants them.
static Census censuses[NUM_SETS];
static int ran = 0;

static const Census *census_of(unsigned ops) {
    if (!ran) {
        for (int i = 0; i < NUM_SETS; i++) census_run(&censuses[i], SETS[i]);
        ran = 1;
    }
    for (int i = 0; i < NUM_SETS; i++) {
        if (censuses[i].ops == ops) return &censuses[i];
    }
    fprintf(stderr, "census_of: no census for ops %#x\n", ops);
    abort();
    return NULL; // not reached; tcc can't see that abort doesn't return
}

static int popcount16(uint32_t s) {
    int n = 0;
    for (int i = 0; i < PLUS_INPUTS; i++) n += (s >> i) & 1;
    return n;
}

TEST(every_witness_decides_its_language_on_the_plain_interpreter) {
    PlusOp ops[CENSUS_MAX_LENGTH];
    long checked = 0;
    for (int k = 0; k < NUM_SETS; k++) {
        const Census *c = census_of(SETS[k]);
        for (uint32_t y = 0; y < PLUS_LANGUAGES; y++) {
            int len = census_witness(c, y, ops);
            EXPECT_EQ(len, c->shortest[y]);
            for (int i = 0; i < len; i++) EXPECT(SETS[k] & CENSUS_OP(ops[i]));
            if (plus_language(ops, len) != y) fprintf(stderr, "  ops %#x, language %u\n", SETS[k], y);
            EXPECT_EQ(plus_language(ops, len), y);
            checked++;
        }
    }
    EXPECT_EQ(checked, NUM_SETS * PLUS_LANGUAGES);
}

// Every program of up to SHORT lines over all four ops, run on the plain
// interpreter: the shortest length each language shows among them is its
// census length, and a language the census puts at SHORT lines or fewer is
// among them. So the census's lengths are exact there, by a count that
// shares nothing with its search.
enum { SHORT = 9 };

TEST(short_programs_enumerated_agree_with_the_census) {
    for (int k = 0; k < NUM_SETS; k++) {
        const Census *c = census_of(SETS[k]);
        int allowed[PLUS_NUM_OPS];
        int num_allowed = 0;
        for (int op = 0; op < PLUS_NUM_OPS; op++) {
            if (SETS[k] & CENSUS_OP(op)) allowed[num_allowed++] = op;
        }
        static int8_t seen[PLUS_LANGUAGES];
        for (int y = 0; y < PLUS_LANGUAGES; y++) seen[y] = -1;
        for (int len = 0; len <= SHORT; len++) {
            long count = 1;
            for (int i = 0; i < len; i++) count *= num_allowed;
            for (long n = 0; n < count; n++) {
                PlusOp ops[SHORT];
                long rest = n;
                for (int i = 0; i < len; i++) {
                    ops[i] = (PlusOp)allowed[rest % num_allowed];
                    rest /= num_allowed;
                }
                uint32_t y = plus_language(ops, len);
                if (seen[y] < 0) seen[y] = (int8_t)len;
            }
        }
        long found = 0;
        for (int y = 0; y < PLUS_LANGUAGES; y++) {
            if (c->shortest[y] <= SHORT) {
                EXPECT_EQ(seen[y], c->shortest[y]);
                found++;
            } else {
                EXPECT_EQ(seen[y], -1);
            }
        }
        long want = 0;
        for (int d = 0; d <= SHORT; d++) want += c->histogram[d];
        EXPECT_EQ(found, want);
    }
}

// `decr` never accepts, and an input still running after the last line
// rejects, so lines of `decr` after a program change no input's verdict.
// Every witness of the four-op census, padded with `decr` to 32 lines,
// packed into a 64-bit word and read back, decides its language: every
// language has a 32-line program of 64 bits, and `nada` is not needed.
TEST(decr_is_a_safe_pad_to_32_lines) {
    const Census *c = census_of(OPS_ALL);
    int longest = 0;
    for (uint32_t y = 0; y < PLUS_LANGUAGES; y++) {
        PlusOp ops[PLUS_MAX_LINES];
        int len = census_witness(c, y, ops);
        if (len > longest) longest = len;
        EXPECT(len <= PLUS_MAX_LINES);
        for (int i = len; i < PLUS_MAX_LINES; i++) ops[i] = PLUS_DECR;
        PlusOp back[PLUS_MAX_LINES];
        plus_unpack(plus_pack(ops, PLUS_MAX_LINES), back);
        for (int i = 0; i < PLUS_MAX_LINES; i++) EXPECT_EQ(back[i], ops[i]);
        EXPECT_EQ(plus_language(back, PLUS_MAX_LINES), y);
    }
    EXPECT_EQ(longest, 28);
}

// With `zero` and `decr` alone, input v is at AX 0 after exactly v `decr`s
// and rejected by the next, so it is accepted when a `zero` stands right
// after its v-th `decr`. A language S needs max(S) `decr`s and one `zero`
// for each member: its shortest program has max(S) + |S| lines, the empty
// language none. The census says so for all 65,536.
TEST(zero_and_decr_alone_take_max_plus_size_lines) {
    const Census *c = census_of(OPS_ZD);
    for (uint32_t y = 0; y < PLUS_LANGUAGES; y++) {
        int want = 0;
        if (y != 0) {
            int max = 0;
            for (int i = 0; i < PLUS_INPUTS; i++) {
                if ((y >> i) & 1) max = i;
            }
            want = max + popcount16(y);
        }
        if (c->shortest[y] != want) fprintf(stderr, "  language %u: shortest %d, max + size %d\n", y, c->shortest[y], want);
        EXPECT_EQ(c->shortest[y], want);
    }
    // {0, ..., 15} is the longest, 15 + 16 lines.
    EXPECT_EQ(c->shortest[PLUS_LANGUAGES - 1], 31);
}

static int is_fibonacci(long n) {
    long a = 1;
    long b = 1;
    while (a < n) {
        long t = a + b;
        a = b;
        b = t;
    }
    return a == n;
}

// For {zero, decr}, length L holds the S with max(S) = m and |S| = L - m:
// C(m, L - m - 1) of them, and the sum over m is the Fibonacci number F(L),
// for every L up to 16, where no m past 15 is wanted. At 17 the term m = 16
// is gone, so the count is F(17) - 1 = 1,596.
//
// For {zero, decr, mod2} the counts at lengths 1 to 15 are Fibonacci numbers
// at every length but 4: they are F(L + 1), except 6 at length 4, where
// F(5) is 5: the three of {zero, decr} ({0, 2}, {1, 2} and {3}), and three
// that end in `mod2, zero`: the even inputs from 2, the odd ones and 0, and
// all 16 (`mod2, zero, decr, zero`). At 16 the count is 1,595, two short of F(17) = 1,597, and from
// 16 to 29 none is Fibonacci; the one language at 30 is a count of 1.
TEST(fibonacci_counts_hold_where_the_census_says) {
    const Census *zd = census_of(OPS_ZD);
    long f[40];
    f[1] = 1;
    f[2] = 1;
    for (int i = 3; i < 40; i++) f[i] = f[i - 1] + f[i - 2];
    for (int L = 1; L <= 16; L++) EXPECT_EQ(zd->histogram[L], f[L]);
    EXPECT_EQ(zd->histogram[17], f[17] - 1);

    const Census *zdm = census_of(OPS_ZDM);
    for (int L = 1; L <= 15; L++) {
        if (L == 4) {
            EXPECT_EQ(zdm->histogram[L], 6);
            EXPECT(!is_fibonacci(zdm->histogram[L]));
        } else {
            EXPECT_EQ(zdm->histogram[L], f[L + 1]);
        }
    }
    EXPECT_EQ(zdm->histogram[16], f[17] - 2);
    for (int L = 16; L < zdm->levels; L++) EXPECT(!is_fibonacci(zdm->histogram[L]));
    EXPECT_EQ(zdm->levels, 30);
    EXPECT_EQ(zdm->histogram[30], 1);

}

TEST_MAIN(TEST_CASE(every_witness_decides_its_language_on_the_plain_interpreter),
          TEST_CASE(short_programs_enumerated_agree_with_the_census),
          TEST_CASE(decr_is_a_safe_pad_to_32_lines),
          TEST_CASE(zero_and_decr_alone_take_max_plus_size_lines),
          TEST_CASE(fibonacci_counts_hold_where_the_census_says))
