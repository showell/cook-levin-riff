// plus_runs.c: the Nibble++ page's oracle, src/plus.c's verdicts on a large
// sample of programs, for web/page_check.mjs to hold the page's wasm to.
//
//   plus_runs <file>
//
// writes one line a program, in hex:
//
//   <lines> <code> <language> <prefix 1> ... <prefix lines>
//
// `code` is the program as plus_pack has it (line 1 in bits 0-1),
// `language` is plus_language's (input n's bit set when it accepts), and
// prefix k is the language of the program's first k lines: an input it
// accepts is one the full run has accepted by line k, so the check holds
// the wasm's every step to it, not only its end.
//
// The sample: every program of 1 to EVERY_UP_TO lines; the four 30-line
// programs of one op each (thirty `decr`s is the page's start); then
// RANDOM_FULL random 30-line programs and RANDOM_MID random programs of 8 to
// 29 lines, from splitmix64 with a fixed seed, so every build checks the
// same ones.

#include "../src/plus.c"

#define EVERY_UP_TO 7
#define PAGE_LINES 30
#define RANDOM_FULL 4000
#define RANDOM_MID 2000

static uint64_t seed = 0x9e3779b97f4a7c15u;

static uint64_t splitmix64(void) {
    uint64_t z = (seed += 0x9e3779b97f4a7c15u);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9u;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebu;
    return z ^ (z >> 31);
}

static void write_program(FILE *out, uint64_t code, int lines) {
    PlusOp ops[PLUS_MAX_LINES];
    plus_unpack(code, ops);
    fprintf(out, "%x %llx %x", lines, (unsigned long long)code, (unsigned)plus_language(ops, lines));
    for (int k = 1; k <= lines; k++) fprintf(out, " %x", (unsigned)plus_language(ops, k));
    fprintf(out, "\n");
}

// The low 2 * lines bits of `word`.
static uint64_t lines_of(uint64_t word, int lines) {
    return lines == 32 ? word : word & (((uint64_t)1 << (2 * lines)) - 1);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: plus_runs <file>\n");
        return 2;
    }
    FILE *out = fopen(argv[1], "w");
    if (out == NULL) {
        perror(argv[1]);
        return 1;
    }
    for (int lines = 1; lines <= EVERY_UP_TO; lines++) {
        for (uint64_t code = 0; code < (uint64_t)1 << (2 * lines); code++) write_program(out, code, lines);
    }
    for (uint64_t op = 0; op < PLUS_NUM_OPS; op++) {
        uint64_t code = 0;
        for (int i = 0; i < PAGE_LINES; i++) code |= op << (2 * i);
        write_program(out, code, PAGE_LINES);
    }
    for (int i = 0; i < RANDOM_FULL; i++) write_program(out, lines_of(splitmix64(), PAGE_LINES), PAGE_LINES);
    for (int i = 0; i < RANDOM_MID; i++) {
        int lines = 8 + (int)(splitmix64() % (PAGE_LINES - 8));
        write_program(out, lines_of(splitmix64(), lines), lines);
    }
    return fclose(out) == 0 ? 0 : 1;
}
