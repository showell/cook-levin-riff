// sat_main.c: the SAT encoding's command line (sat.h). A unity build, as
// main.c is.
//
//   sat LANG [len=N] [fix=K:OP]...   puzzle LANG's CNF, DIMACS, on stdout:
//                                    lines past N forced to nada, line K
//                                    (1..6) locked to OP (nada, zero, decr,
//                                    mod2)
//   sat facts                        one line a language, from the machine:
//                                    `LANG COUNT SHORTEST VARS CLAUSES`, its
//                                    solving programs, the shortest one's
//                                    length, and its CNF's size
//   sat solves LANG CODE             whether program CODE (vm.h's code)
//                                    solves LANG on the machine: prints yes
//                                    and exits 0, or no and exits 1

#include "vm.c"
#include "sat.c"

#include <string.h>

static int usage(void) {
    fprintf(stderr, "usage: sat LANG [len=N] [fix=K:OP]... | sat facts | sat solves LANG CODE\n");
    return 2;
}

// A whole decimal number in [lo, hi], or -1.
static long number(const char *s, long lo, long hi) {
    if (*s == 0) return -1;
    long v = 0;
    for (const char *t = s; *t; t++) {
        if (*t < '0' || *t > '9' || v > hi) return -1;
        v = 10 * v + (*t - '0');
    }
    return v < lo || v > hi ? -1 : v;
}

static int program_length(uint32_t code) {
    Program p = disassemble(code);
    int len = MAX_STEPS;
    while (len > 0 && p.ops[len - 1] == OP_NADA) len--;
    return len;
}

static int facts(void) {
    for (int y = 0; y < NUM_LANGUAGES; y++) {
        int count = 0, shortest = MAX_STEPS + 1;
        for (uint32_t n = 0; n < NUM_PROGRAMS; n++) {
            if (!sat_solves(y, n)) continue;
            count++;
            if (program_length(n) < shortest) shortest = program_length(n);
        }
        SatPuzzle p = sat_puzzle(y);
        Cnf c = sat_encode(&p);
        printf("%d %d %d %d %zu\n", y, count, shortest, c.num_vars, c.num_clauses);
        sat_free(&c);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "facts") == 0) return facts() || fflush(stdout) != 0;
    if (argc == 4 && strcmp(argv[1], "solves") == 0) {
        long lang = number(argv[2], 0, NUM_LANGUAGES - 1);
        long code = number(argv[3], 0, NUM_PROGRAMS - 1);
        if (lang < 0 || code < 0) return usage();
        int yes = sat_solves((int)lang, (uint32_t)code);
        printf("%s\n", yes ? "yes" : "no");
        return yes ? 0 : 1;
    }
    if (argc < 2) return usage();
    long lang = number(argv[1], 0, NUM_LANGUAGES - 1);
    if (lang < 0) return usage();
    SatPuzzle p = sat_puzzle((int)lang);
    for (int a = 2; a < argc; a++) {
        if (strncmp(argv[a], "len=", 4) == 0) {
            long n = number(argv[a] + 4, 0, MAX_STEPS);
            if (n < 0) return usage();
            p.max_len = (int)n;
        } else if (strncmp(argv[a], "fix=", 4) == 0 && argv[a][4] >= '1' && argv[a][4] <= '6' && argv[a][5] == ':') {
            int k = argv[a][4] - '0';
            Op op = op_from_name(argv[a] + 6);
            uint32_t place = 1;
            for (int i = 1; i < k; i++) place *= NUM_OPS;
            if (p.fixed >> (k - 1) & 1) return usage();
            p.fixed |= 1 << (k - 1);
            p.fixed_ops += (uint32_t)op * place;
        } else {
            return usage();
        }
    }
    Cnf c = sat_encode(&p);
    int ok = sat_write_dimacs(stdout, &c, &p);
    sat_free(&c);
    return ok && fflush(stdout) == 0 ? 0 : 1;
}
