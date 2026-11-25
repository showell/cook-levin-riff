// puzzles.c: the playground's tables, from every one of the 4,096 programs.
//
//   puzzles   the opcodes, each with its name and its one-line comment, and
//             the puzzles, easiest first, as JSON: each language's name, its
//             members, how many programs accept exactly it (`count`), and
//             the length of the shortest (`shortest`) with one such program
//             (`example`, its code).
//   runs      run_progam's Status for every program on every input, as JSON:
//             a string of 4,096 x 4 digits, program-major, for the check to
//             hold the wasm to.
//
// A program's length is its instructions before its trailing `nada`s: a
// program of all `nada`s has length 0. Puzzles go by count, most first; a
// tie by shortest length, then by code.

#include "../src/vm.c"

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
    if (a->count != b->count) return a->count > b->count;
    if (a->shortest != b->shortest) return a->shortest < b->shortest;
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
        printf("], \"count\": %d, \"shortest\": %d, \"example\": %u}%s\n", z.count, z.shortest, z.example,
               i + 1 < NUM_LANGUAGES ? "," : "");
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
