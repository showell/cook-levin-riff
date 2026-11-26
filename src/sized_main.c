// sized_main.c: the examples of the book's chapter "Nibble, sized", members
// and non-members of ACCEPTS, DECIDES and SOLVABLE at the toy's size, n = 2
// and k = 6. book/book.shale builds it, runs it and holds the book's source
// to each table it prints, a blank line between them. Each verdict comes
// from the machine (vm.c): a run for ACCEPTS, four runs for DECIDES, and
// the search of all 4,096 programs for SOLVABLE, whose evidence is the
// committed SAT file that settles the same question (sat/proofs/, checked
// by book.shale's check_solver_files); a file named here and missing stops
// the program.
// A unity build: this file includes the C it needs, so it is one compile.

#include "vm.c"

// A program's length: its instructions before its trailing `nada`s.
static int length_of(Program p) {
    int len = MAX_STEPS;
    while (len > 0 && p.ops[len - 1] == OP_NADA) len--;
    return len;
}

// The program as the book writes it in backticks: its ops before the
// trailing `nada`s.
static void print_ops(Program p) {
    int len = length_of(p);
    printf("`");
    for (int i = 0; i < len; i++) printf("%s%s", i == 0 ? "" : ", ", op_name(p.ops[i]));
    printf("`");
}

// The program's 12 bits, two a line, line 1 first.
static void print_program_bits(Program p) {
    for (int i = 0; i < MAX_STEPS; i++) printf("%s%d%d", i == 0 ? "" : " ", (p.ops[i] >> 1) & 1, p.ops[i] & 1);
}

// A set's 4 want bits, input 0's bit first.
static void print_want(uint32_t code) {
    for (int n = 0; n < NUM_INPUTS; n++) printf("%u", (code >> n) & 1);
}

static void print_set(uint32_t code) {
    Lang l = language(code);
    printf("{");
    for (size_t i = 0; i < l.len; i++) printf("%s%d", i == 0 ? "" : ", ", l.items[i]);
    printf("}");
}

static Program program_of(const Op *ops, int count) {
    Program p;
    for (int i = 0; i < MAX_STEPS; i++) p.ops[i] = i < count ? ops[i] : OP_NADA;
    return p;
}

static uint32_t decided(Program p) {
    return encoded_language(get_language_that_program_accepts(p));
}

static void require_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "sized_main: no file %s\n", path);
        exit(1);
    }
    fclose(f);
}

static void accepts_row(Program p, int x) {
    Machine m = vm_start(x);
    int line = 0;
    for (int i = 0; i < MAX_STEPS && !m.halted; i++) {
        m = vm_step(m, p.ops[i]);
        if (m.halted) line = i + 1;
    }
    int in = run_progam(x, p) == STATUS_ACCEPTED;
    printf("| ");
    print_ops(p);
    printf(" | %d | `", x);
    print_program_bits(p);
    printf(" %d%d` | ", (x >> 1) & 1, x & 1);
    if (line == 0) {
        printf("runs past line %d, rejected", MAX_STEPS);
    } else {
        printf("`%s` %s it at line %d", op_name(p.ops[line - 1]), in ? "accepts" : "rejects", line);
    }
    printf(" | %s |\n", in ? "in" : "out");
}

static void decides_row(Program p, uint32_t want) {
    uint32_t got = decided(p);
    printf("| ");
    print_ops(p);
    printf(" | ");
    print_set(want);
    printf(" | `");
    print_program_bits(p);
    printf(" ");
    print_want(want);
    printf("` | ");
    print_set(got);
    printf(" | %s |\n", got == want ? "in" : "out");
}

static void solvable_row(uint32_t want, int k) {
    int shortest = MAX_STEPS + 1;
    Program first = disassemble(0);
    for (uint32_t n = 0; n < NUM_PROGRAMS; n++) {
        Program p = disassemble(n);
        if (decided(p) == want && length_of(p) < shortest) {
            shortest = length_of(p);
            first = p;
        }
    }
    int in = k >= shortest;
    char path[64];
    snprintf(path, sizeof path, "sat/proofs/%u-len%d.%s", want, k, in ? "model" : "drat");
    require_file(path);
    printf("| ");
    print_set(want);
    printf(" | %d | `", k);
    print_want(want);
    printf(" ");
    for (int i = 0; i < k; i++) printf("1");
    printf("` | ");
    if (in) {
        print_ops(first);
        printf(", and `%s`", path);
    } else {
        printf("the shortest has %d lines, and `%s`", shortest, path);
    }
    printf(" | %s |\n", in ? "in" : "out");
}

int main(void) {
    const Op two[] = {OP_MOD2, OP_ZERO};
    Program p = program_of(two, 2);

    printf("| P | x | (P, x) as one string | The run | In ACCEPTS |\n|---|---:|---|---|---|\n");
    accepts_row(p, 2);
    accepts_row(p, 1);
    printf("\n");

    printf("| P | W | (P, W) as one string | What P decides | In DECIDES |\n|---|---|---|---|---|\n");
    decides_row(p, 5);
    decides_row(p, 1);
    printf("\n");

    printf("| W | k | (W, k) as one string | Evidence | In SOLVABLE |\n|---|---:|---|---|---|\n");
    solvable_row(5, 2);
    solvable_row(5, 1);
    solvable_row(13, 6);
    solvable_row(13, 5);
    return fflush(stdout) == 0 ? 0 : 1;
}
