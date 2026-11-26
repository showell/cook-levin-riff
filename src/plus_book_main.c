// plus_book_main.c: the tables of the book's chapter "Nibble++", from the
// census (census.h). book/book.shale builds it, runs it and holds the
// book's source to each block it prints, a blank line between blocks:
// the four opcode sets side by side, the histogram of shortest lengths with
// all four ops, the {zero, decr} counts beside the Fibonacci numbers, and
// some languages' shortest lengths under each set, with witnesses, and the
// hardest languages, of the longest shortest length, with one witness in
// full.
// A unity build: this file includes the C it needs, so it is one compile.

#include "plus.c"
#include "census.c"

enum { SETS = 4, ALL_FOUR = 3, ZERO_DECR = 0, ZERO_DECR_MOD2 = 1 };

// Static: a census holds two 64 Ki tables beside its state arrays.
static Census census;
static int8_t shortest[SETS][PLUS_LANGUAGES];

static const unsigned SET_OPS[SETS] = {
    CENSUS_OP(PLUS_ZERO) | CENSUS_OP(PLUS_DECR),
    CENSUS_OP(PLUS_ZERO) | CENSUS_OP(PLUS_DECR) | CENSUS_OP(PLUS_MOD2),
    CENSUS_OP(PLUS_ZERO) | CENSUS_OP(PLUS_DECR) | CENSUS_OP(PLUS_HALF),
    CENSUS_OP(PLUS_ZERO) | CENSUS_OP(PLUS_DECR) | CENSUS_OP(PLUS_MOD2) | CENSUS_OP(PLUS_HALF),
};

// A language of the examples table, as the book names it.
typedef struct {
    const char *name;
    uint32_t code;
} Example;

static uint32_t set_of(const int *inputs, int count) {
    uint32_t code = 0;
    for (int i = 0; i < count; i++) code |= 1u << inputs[i];
    return code;
}

static void print_set_name(unsigned ops) {
    char name[64];
    census_ops_name(ops, name, sizeof name);
    printf("{");
    for (const char *p = name; *p != '\0'; p++) {
        if (*p == ',') {
            printf(", ");
        } else {
            putchar(*p);
        }
    }
    printf("}");
}

// A formal language's members, `{0, ..., 10, 12, 15}`: a run of three or
// more as its ends, as book/book.shale's members_named writes them.
static void print_members(uint32_t lang) {
    printf("{");
    int first = 1;
    int n = 0;
    while (n < PLUS_INPUTS) {
        if (!(lang >> n & 1)) {
            n++;
            continue;
        }
        int end = n;
        while (end + 1 < PLUS_INPUTS && (lang >> (end + 1) & 1)) end++;
        printf("%s", first ? "" : ", ");
        first = 0;
        if (end - n >= 2) {
            printf("%d, ..., %d", n, end);
        } else {
            for (int k = n; k <= end; k++) printf("%s%d", k == n ? "" : ", ", k);
        }
        n = end + 1;
    }
    printf("}");
}

static long fibonacci(int n) {
    long a = 0;
    long b = 1;
    for (int i = 0; i < n; i++) {
        long t = a + b;
        a = b;
        b = t;
    }
    return a;
}

int main(void) {
    long histogram[SETS][CENSUS_MAX_LENGTH + 1];
    int longest[SETS];
    size_t states[SETS];
    long total[SETS];
    for (int s = 0; s < SETS; s++) {
        census_run(&census, SET_OPS[s]);
        states[s] = census.num_states;
        longest[s] = 0;
        total[s] = 0;
        for (int d = 0; d <= CENSUS_MAX_LENGTH; d++) {
            histogram[s][d] = census.histogram[d];
            if (census.histogram[d] > 0) longest[s] = d;
            total[s] += census.histogram[d] * d;
        }
        memcpy(shortest[s], census.shortest, sizeof shortest[s]);
        census_free(&census);
    }

    printf("| Opcodes | States reached | Mean shortest length | Longest |\n|---|---:|---:|---:|\n");
    for (int s = 0; s < SETS; s++) {
        printf("| ");
        print_set_name(SET_OPS[s]);
        printf(" | %zu | %.2f | %d |\n", states[s], (double)total[s] / PLUS_LANGUAGES, longest[s]);
    }
    printf("\n");

    printf("| Shortest length | Formal languages |\n|---:|---:|\n");
    for (int d = 0; d <= longest[ALL_FOUR]; d++) printf("| %d | %ld |\n", d, histogram[ALL_FOUR][d]);
    printf("\n");

    printf("| Length L | {zero, decr} | F(L) | {zero, decr, mod2} |\n|---:|---:|---:|---:|\n");
    for (int d = 1; d <= 17; d++) {
        printf("| %d | %ld | %ld | %ld |\n", d, histogram[ZERO_DECR][d], fibonacci(d), histogram[ZERO_DECR_MOD2][d]);
    }
    printf("\n");

    const int top_half[] = {8, 9, 10, 11, 12, 13, 14, 15};
    const int bit_one[] = {2, 3, 6, 7, 10, 11, 14, 15};
    const int fifteen[] = {15};
    const Example examples[] = {
        {"{15}", set_of(fifteen, 1)},
        {"{8, ..., 15}", set_of(top_half, 8)},
        {"{2, 3, 6, 7, 10, 11, 14, 15}", set_of(bit_one, 8)},
        {"all but 13", 0xFFFFu & ~(1u << 13)},
        {"all 16", 0xFFFFu},
    };
    const int num_examples = (int)(sizeof examples / sizeof examples[0]);
    printf("| Formal language |");
    for (int s = 0; s < SETS; s++) {
        printf(" ");
        print_set_name(SET_OPS[s]);
        printf(" |");
    }
    printf("\n|---|---:|---:|---:|---:|\n");
    for (int e = 0; e < num_examples; e++) {
        printf("| %s |", examples[e].name);
        for (int s = 0; s < SETS; s++) printf(" %d |", shortest[s][examples[e].code]);
        printf("\n");
    }
    printf("\n");

    // The witnesses with all four ops, for the examples of eight lines or
    // fewer; the census is run again for them.
    census_run(&census, SET_OPS[ALL_FOUR]);
    printf("```text\n");
    for (int e = 0; e < num_examples; e++) {
        PlusOp ops[CENSUS_MAX_LENGTH];
        int len = census_witness(&census, examples[e].code, ops);
        if (len > 8) continue;
        printf("%s:", examples[e].name);
        for (int i = 0; i < len; i++) printf("%s %s", i == 0 ? "" : ",", plus_op_name(ops[i]));
        printf("\n");
    }
    printf("```\n\n");

    // The hardest formal languages, those of the longest shortest length,
    // each with the inputs it leaves out; then the witness of the last of
    // them, all but 13, in full, one opcode a line.
    int hardest = longest[ALL_FOUR];
    printf("| The formal languages of %d lines | Left out |\n|---|---|\n", hardest);
    uint32_t last = 0;
    for (uint32_t w = 0; w < PLUS_LANGUAGES; w++) {
        if (census.shortest[w] != hardest) continue;
        printf("| ");
        print_members(w);
        printf(" | ");
        int first = 1;
        for (int n = 0; n < PLUS_INPUTS; n++) {
            if (w >> n & 1) continue;
            printf("%s%d", first ? "" : ", ", n);
            first = 0;
        }
        printf(" |\n");
        last = w;
    }
    printf("\n```text\n");
    PlusOp ops[CENSUS_MAX_LENGTH];
    int len = census_witness(&census, last, ops);
    for (int i = 0; i < len; i++) printf("%s\n", plus_op_name(ops[i]));
    printf("```\n");
    census_free(&census);
    return fflush(stdout) == 0 ? 0 : 1;
}
