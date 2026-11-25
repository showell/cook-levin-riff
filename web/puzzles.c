// puzzles.c: the playground's tables, from every one of the 4,096 programs.
//
//   puzzles   the opcodes, each with its name and its one-line comment, and
//             the puzzles, easiest first, as JSON: each language's name, its
//             members, how many programs accept exactly it (`count`), and
//             the length of the shortest (`shortest`) with one such program
//             (`example`, its code); then the page's: how many lines are
//             choosable (`lines`, the rest locked to nada) and the program
//             a puzzle opens with (`start`).
//   runs      run_progam's Status for every program on every input, as JSON:
//             a string of 4,096 x 4 digits, program-major, for the check to
//             hold the wasm to.
//
// A program's length is its instructions before its trailing `nada`s: a
// program of all `nada`s has length 0. The first five puzzles are the
// lessons, in LESSONS' order, each with a sentence on the opcode it teaches
// (`lesson`). A puzzle's choosable lines are its shortest length and it
// opens as all nada, but for the nada lesson: all nada solves it, so it has
// one line, which opens as `zero`. The other eleven ramp up: by shortest length, fewest lines
// first; a tie by count, most first, then by code.

#include "../src/vm.c"

#define NUM_LESSONS 5

typedef struct {
    int code;
    const char *text;
    int lines;      // choosable lines; 0 for the shortest length
    uint32_t start; // the program it opens with
} Lesson;

// The languages' codes are their members' bits: {0} is 1, {1} is 2, {0, 2}
// is 5, {2} is 4, { } is 0.
static const Lesson LESSONS[NUM_LESSONS] = {
    {1, "zero accepts when AX is 0; otherwise the run goes on to the next line.", 0, 0},
    {2, "decr takes 1 from AX, and rejects when AX is already 0.", 0, 0},
    {5, "mod2 takes 2 from AX when AX is 2 or 3.", 0, 0},
    {4, "decr twice: the trace shows decr rejecting the inputs that reach 0 too soon.", 0, 0},
    {0, "nada does nothing, and a run that ends without accepting rejects: "
        "a program of nothing but nada accepts no input.",
     1, 1},
};

// The lesson's place in LESSONS, or NUM_LESSONS for a puzzle that isn't one.
static int lesson_of(int code) {
    for (int i = 0; i < NUM_LESSONS; i++) {
        if (LESSONS[i].code == code) return i;
    }
    return NUM_LESSONS;
}

static const char *const NAMES[NUM_LANGUAGES] = {
    "always_false", "is_tiny",    "is_one",     "is_small",    "is_two",    "is_even",
    "is_middling",  "is_not_huge", "is_huge",   "is_extreme",  "is_odd",    "is_not_two",
    "is_big",       "is_not_one",  "is_not_tiny", "always_true",
};

static int program_length(Program p) {
    int len = MAX_STEPS;
    while (len > 0 && p.ops[len - 1] == OP_NADA) len--;
    return len;
}

typedef struct {
    int code;
    int count;
    int shortest;
    uint32_t example;
} Puzzle;

static int easier(const Puzzle *a, const Puzzle *b) {
    int la = lesson_of(a->code), lb = lesson_of(b->code);
    if (la != lb) return la < lb;
    if (a->shortest != b->shortest) return a->shortest < b->shortest;
    if (a->count != b->count) return a->count > b->count;
    return a->code < b->code;
}

static void print_puzzles(void) {
    Puzzle puzzles[NUM_LANGUAGES];
    for (int y = 0; y < NUM_LANGUAGES; y++) {
        Puzzle none = {y, 0, MAX_STEPS + 1, 0};
        puzzles[y] = none;
    }
    for (uint32_t n = 0; n < NUM_PROGRAMS; n++) {
        Program p = disassemble(n);
        Puzzle *z = &puzzles[encoded_language(get_language_that_program_accepts(p))];
        z->count++;
        int len = program_length(p);
        if (len < z->shortest) {
            z->shortest = len;
            z->example = n;
        }
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
        if (z.count == 0) {
            fprintf(stderr, "puzzles: no program accepts language %d\n", z.code);
            exit(1);
        }
        Lang lang = language((uint32_t)z.code);
        printf("  {\"code\": %d, \"name\": \"%s\", \"set\": [", z.code, NAMES[z.code]);
        for (size_t k = 0; k < lang.len; k++) printf("%s%d", k ? ", " : "", lang.items[k]);
        printf("], \"count\": %d, \"shortest\": %d, \"example\": %u", z.count, z.shortest, z.example);
        int l = lesson_of(z.code);
        int lines = l < NUM_LESSONS && LESSONS[l].lines ? LESSONS[l].lines : z.shortest;
        uint32_t start = l < NUM_LESSONS ? LESSONS[l].start : 0;
        printf(", \"lines\": %d, \"start\": %u", lines, start);
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
