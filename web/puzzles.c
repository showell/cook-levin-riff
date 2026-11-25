// puzzles.c: the playground's tables, from every one of the 4,096 programs.
//
//   puzzles   the opcodes, each with its name and its one-line comment, and
//             the 16 puzzles, one a language, easiest first, as JSON. A
//             puzzle names its language's members (`set`): accept exactly
//             those and reject the rest. Each has its name, how many programs
//             solve it (`count`), and the length of the shortest (`shortest`);
//             then the page's: how many lines are open (`lines`, the rest
//             locked to nada), how many of those are given (`given`, the
//             first lines, locked to their op in `start`), the program it
//             opens with (`start`), and one program that solves it within
//             those (`example`, its code: the shortest whose given lines are
//             `start`'s).
//   runs      run_progam's Status for every program on every input, as JSON:
//             a string of 4,096 x 4 digits, program-major, for the check to
//             hold the wasm to.
//
// A program's length is its instructions before its trailing `nada`s: a
// program of all `nada`s has length 0. The first four puzzles are the
// lessons, in LESSONS' order, each with a sentence on the opcode it shows
// (`lesson`): is_tiny ({0}, zero), is_one ({1}, line 1 given as decr),
// is_even ({0, 2}, line 1 given as mod2) and grumpy ({ }, one line opening as
// zero, which nada, decr or mod2 solves). The other twelve ramp up: by
// shortest length, fewest lines first; a tie by count, most first, then by
// code. Each opens as all nada with its shortest length open.

#include "../src/vm.c"

#define NUM_LESSONS 4

typedef struct {
    int code;
    const char *text;
    int lines;      // open lines
    int given;      // the first lines, locked to start's ops
    uint32_t start; // the program it opens with
} Lesson;

// The languages' codes are their members' bits: {0} is 1, { } is 0.
static const Lesson LESSONS[NUM_LESSONS] = {
    {1, "zero accepts when AX is 0; otherwise the run goes on to the next line.", 1, 0, 0},
    {2, "decr takes 1 from AX, and rejects when AX is already 0.", 2, 1, OP_DECR},
    {5, "mod2 takes 2 from AX when AX is 2 or 3, which leaves AX's last bit.", 2, 1, OP_MOD2},
    {0, "Nibble at its grumpiest: nada, decr and mod2 never accept on their own, so any of the three on line 1 "
        "turns every input away.",
     1, 0, OP_ZERO},
};

// The lesson's place in LESSONS, or NUM_LESSONS for a puzzle that isn't one.
static int lesson_of(int code) {
    for (int i = 0; i < NUM_LESSONS; i++) {
        if (LESSONS[i].code == code) return i;
    }
    return NUM_LESSONS;
}

static const char *const NAMES[NUM_LANGUAGES] = {
    "grumpy",      "is_tiny",    "is_one",     "is_small",    "is_two",    "is_even",
    "is_middling", "is_not_huge", "is_huge",   "is_extreme",  "is_odd",    "is_not_two",
    "is_big",      "is_not_one",  "is_not_tiny", "always_true",
};

static int program_length(Program p) {
    int len = MAX_STEPS;
    while (len > 0 && p.ops[len - 1] == OP_NADA) len--;
    return len;
}

// Program n's op on line k (from 0).
static int op_at(uint32_t n, int k) {
    for (int i = 0; i < k; i++) n /= NUM_OPS;
    return (int)(n % NUM_OPS);
}

typedef struct {
    int code;
    int count;
    int shortest;
    int lines;
    int given;
    uint32_t start;
    int fits;       // programs that solve it within its lines and given ones
    int example_len;
    uint32_t example;
} Puzzle;

static int easier(const Puzzle *a, const Puzzle *b) {
    int la = lesson_of(a->code), lb = lesson_of(b->code);
    if (la != lb) return la < lb;
    if (a->shortest != b->shortest) return a->shortest < b->shortest;
    if (a->count != b->count) return a->count > b->count;
    return a->code < b->code;
}

// Program n fits puzzle z's page: nothing past its open lines, and its given
// lines as `start` has them.
static int fits(const Puzzle *z, uint32_t n, int len) {
    if (len > z->lines) return 0;
    for (int k = 0; k < z->given; k++) {
        if (op_at(n, k) != op_at(z->start, k)) return 0;
    }
    return 1;
}

static void fail_puzzle(const char *why, int code) {
    fprintf(stderr, "puzzles: %s (puzzle %s)\n", why, NAMES[code]);
    exit(1);
}

static void print_puzzles(void) {
    Puzzle puzzles[NUM_LANGUAGES];
    int accepts[NUM_PROGRAMS];
    int lens[NUM_PROGRAMS];
    for (int y = 0; y < NUM_LANGUAGES; y++) {
        Puzzle none = {y, 0, MAX_STEPS + 1, 0, 0, 0, 0, MAX_STEPS + 1, 0};
        puzzles[y] = none;
    }
    for (uint32_t n = 0; n < NUM_PROGRAMS; n++) {
        Program p = disassemble(n);
        lens[n] = program_length(p);
        accepts[n] = (int)encoded_language(get_language_that_program_accepts(p));
        Puzzle *z = &puzzles[accepts[n]];
        z->count++;
        if (lens[n] < z->shortest) z->shortest = lens[n];
    }
    for (int y = 0; y < NUM_LANGUAGES; y++) {
        Puzzle *z = &puzzles[y];
        int l = lesson_of(y);
        z->lines = l < NUM_LESSONS ? LESSONS[l].lines : z->shortest;
        z->given = l < NUM_LESSONS ? LESSONS[l].given : 0;
        z->start = l < NUM_LESSONS ? LESSONS[l].start : 0;
        if (z->given >= z->lines) fail_puzzle("no open line is left to choose", y);
        if (program_length(disassemble(z->start)) > z->lines) fail_puzzle("its start has an op on a locked line", y);
        if (accepts[z->start] == y) fail_puzzle("its start solves it", y);
        for (uint32_t n = 0; n < NUM_PROGRAMS; n++) {
            if (accepts[n] != y || !fits(z, n, lens[n])) continue;
            z->fits++;
            if (lens[n] < z->example_len) {
                z->example_len = lens[n];
                z->example = n;
            }
        }
        if (z->fits == 0) fail_puzzle("no program solves it within its lines", y);
    }
    // Sixteen: an insertion sort.
    for (int i = 1; i < NUM_LANGUAGES; i++) {
        Puzzle z = puzzles[i];
        int j = i;
        while (j > 0 && easier(&z, &puzzles[j - 1])) {
            puzzles[j] = puzzles[j - 1];
            j--;
        }
        puzzles[j] = z;
    }
    printf("{\"ops\": [");
    for (int i = 0; i < NUM_OPS; i++) {
        printf("%s{\"name\": \"%s\", \"comment\": \"%s\"}", i ? ", " : "", op_name((Op)i), op_comment((Op)i));
    }
    printf("],\n\"puzzles\": [\n");
    for (int i = 0; i < NUM_LANGUAGES; i++) {
        Puzzle z = puzzles[i];
        Lang lang = language((uint32_t)z.code);
        printf("  {\"code\": %d, \"name\": \"%s\", \"set\": [", z.code, NAMES[z.code]);
        for (size_t k = 0; k < lang.len; k++) printf("%s%d", k ? ", " : "", lang.items[k]);
        printf("], \"count\": %d, \"shortest\": %d, \"example\": %u", z.count, z.shortest, z.example);
        printf(", \"lines\": %d, \"given\": %d, \"start\": %u", z.lines, z.given, z.start);
        int l = lesson_of(z.code);
        if (l < NUM_LESSONS) printf(", \"lesson\": \"%s\"", LESSONS[l].text);
        printf("}%s\n", i + 1 < NUM_LANGUAGES ? "," : "");
    }
    printf("]}\n");
}

static void print_runs(void) {
    printf("\"");
    for (uint32_t n = 0; n < NUM_PROGRAMS; n++) {
        Program p = disassemble(n);
        for (int i = 0; i < NUM_INPUTS; i++) printf("%d", (int)run_progam(i, p));
    }
    printf("\"\n");
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "puzzles") == 0) {
        print_puzzles();
    } else if (argc == 2 && strcmp(argv[1], "runs") == 0) {
        print_runs();
    } else {
        fprintf(stderr, "usage: puzzles puzzles|runs\n");
        return 2;
    }
    return fflush(stdout) == 0 ? 0 : 1;
}
