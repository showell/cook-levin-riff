// puzzles.c: the playground's tables, from every one of the 4,096 programs.
//
//   puzzles   the opcodes, each with its name and its one-line comment, and
//             the 16 puzzles, one a language, easiest first, as JSON. A
//             puzzle names its language's members (`set`): accept exactly
//             those and reject the rest. Each has its name, how many programs
//             solve it (`count`), and the length of the shortest (`shortest`);
//             then the page's: how many lines are open (`lines`, the rest
//             locked to nada), which of those are fixed (`fixed`, a bitmask,
//             line k's bit 1 << (k - 1), each locked to its op in `start`),
//             the program it opens with (`start`), and one program that
//             solves it within those (`example`, its code: a shortest whose
//             fixed lines are `start`'s).
//   runs      run_progam's Status for every program on every input, as JSON:
//             a string of 4,096 x 4 digits, program-major, for the check to
//             hold the wasm to.
//
// A program's length is its instructions before its trailing `nada`s: a
// program of all `nada`s has length 0. The first four puzzles are the
// lessons, in LESSONS' order, each with a sentence on the opcode it shows
// (`lesson`): is_tiny ({0}, zero), is_one ({1}, line 1 fixed as decr),
// is_even ({0, 2}, line 1 fixed as mod2) and grumpy ({ }, one line opening as
// zero, which nada, decr or mod2 solves). The other twelve ramp up by effort
// (`easier`): the shortest length with each mod2 counted as a line more, so
// decr, decr, zero comes before is_odd's mod2. Each has its shortest length open, and opens as all nada, but for
// a hint: when some shortest program ends in zero, its last line is fixed
// as zero.

#include "../src/vm.c"

#define NUM_LESSONS 4

typedef struct {
    int code;
    const char *text;
    int lines;      // open lines
    int fixed;      // the lines locked to start's ops, line k (from 0) bit k
    uint32_t start; // the program it opens with
} Lesson;

// The languages' codes are their members' bits: {0} is 1, { } is 0.
static const Lesson LESSONS[NUM_LESSONS] = {
    // fixed: is_one's and is_even's line 1 (bit 0).
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
    int fixed;
    uint32_t start;
    int fits;       // programs that solve it within its lines and fixed ones
    int example_len;
    uint32_t example;
    int mod2s; // in its plainest shortest program: fewest mod2s, then zeros
    int zeros;
} Puzzle;

// How hard a puzzle is to think through: its shortest length, a mod2 counted
// as a line more, since it is the op that takes more thought (Steve:
// "decr, decr, zero ... is simpler for my brain than is_odd").
static int effort(const Puzzle *z) { return z->shortest + z->mod2s; }

// Lessons first, in order; then by effort, a tie going to the puzzle with no
// mod2, then fewer zeros (fewer places it can accept), then more solutions.
static int easier(const Puzzle *a, const Puzzle *b) {
    int la = lesson_of(a->code), lb = lesson_of(b->code);
    if (la != lb) return la < lb;
    if (effort(a) != effort(b)) return effort(a) < effort(b);
    if (a->mod2s != b->mod2s) return a->mod2s < b->mod2s;
    if (a->zeros != b->zeros) return a->zeros < b->zeros;
    if (a->count != b->count) return a->count > b->count;
    return a->code < b->code;
}

// How many of program n's first len ops are op.
static int ops_of(uint32_t n, int len, int op) {
    int k = 0;
    for (int i = 0; i < len; i++) k += op_at(n, i) == op;
    return k;
}

// Program n fits puzzle z's page: nothing past its open lines, and its fixed
// lines as `start` has them.
static int fits(const Puzzle *z, uint32_t n, int len) {
    if (len > z->lines) return 0;
    for (int k = 0; k < z->lines; k++) {
        if ((z->fixed >> k & 1) && op_at(n, k) != op_at(z->start, k)) return 0;
    }
    return 1;
}

// Some shortest program that solves language y ends in zero.
static int shortest_ends_in_zero(const int *accepts, const int *lens, int y, int shortest) {
    if (shortest == 0) return 0;
    for (uint32_t n = 0; n < NUM_PROGRAMS; n++) {
        if (accepts[n] == y && lens[n] == shortest && op_at(n, shortest - 1) == OP_ZERO) return 1;
    }
    return 0;
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
        Puzzle none = {y, 0, MAX_STEPS + 1, 0, 0, 0, 0, MAX_STEPS + 1, 0, MAX_STEPS + 1, MAX_STEPS + 1};
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
    for (uint32_t n = 0; n < NUM_PROGRAMS; n++) {
        Puzzle *z = &puzzles[accepts[n]];
        if (lens[n] != z->shortest) continue;
        int m = ops_of(n, lens[n], OP_MOD2), zs = ops_of(n, lens[n], OP_ZERO);
        if (m < z->mod2s || (m == z->mod2s && zs < z->zeros)) {
            z->mod2s = m;
            z->zeros = zs;
        }
    }
    for (int y = 0; y < NUM_LANGUAGES; y++) {
        Puzzle *z = &puzzles[y];
        int l = lesson_of(y);
        if (l < NUM_LESSONS) {
            z->lines = LESSONS[l].lines;
            z->fixed = LESSONS[l].fixed;
            z->start = LESSONS[l].start;
        } else {
            // The hint: a shortest program's final zero, fixed.
            z->lines = z->shortest;
            z->fixed = 0;
            z->start = 0;
            if (shortest_ends_in_zero(accepts, lens, y, z->shortest)) {
                z->fixed = 1 << (z->lines - 1);
                uint32_t place = 1;
                for (int k = 0; k < z->lines - 1; k++) place *= NUM_OPS;
                z->start = OP_ZERO * place;
            }
        }
        if (z->fixed >> z->lines) fail_puzzle("a fixed line is past its open lines", y);
        int picks = 0;
        for (int k = 0; k < z->lines; k++) {
            if (!(z->fixed >> k & 1)) picks++;
            else if (op_at(z->start, k) == OP_NADA) fail_puzzle("a fixed line's op isn't in its start", y);
        }
        if (picks == 0) fail_puzzle("no open line is left to choose", y);
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
        if (z->example_len != z->shortest) fail_puzzle("no shortest program fits its fixed lines", y);
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
        printf(", \"lines\": %d, \"fixed\": %d, \"start\": %u", z.lines, z.fixed, z.start);
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
