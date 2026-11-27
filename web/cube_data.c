// cube_data.c: the cube page's data, from cube/cube.c and the census.
//
//   cube_data pars PARS BIN   reads the census's every pattern's par and
//                             solution (census 4 PARS, a line a pattern, in
//                             pattern order) and writes BIN, the page's
//                             table: 65,536 bytes, byte p the par of
//                             pattern p (tile i lit when bit i is set)
//   cube_data runs OUT        writes cube.c's runs of 400 sample programs to
//                             OUT, for web/cube_check.mjs to hold the Roc
//                             wasm to, frame by frame
//
// A line of OUT is a program, `-` for the empty one, then each frame after
// each roll as `cell.face.lit` (cube.h's numbers, the lit set in decimal);
// a program with a roll off the board, which halts the machine, ends
// `halt@k`, k the rolls before it; the rolls after it do nothing.
// The programs are the census's sample levels, then 396 pseudo-random ones
// of 1 to 80 rolls, most rolls kept on the board so that runs are long and
// a few let off it.

#include "../cube/cube.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { N = 4, PATTERNS = 1 << (N * N), MAX_ROLLS = 80, SAMPLES = 400 };

static int pars(const char *from, const char *to) {
    FILE *in = fopen(from, "r");
    if (in == NULL) {
        fprintf(stderr, "cube_data: cannot read %s\n", from);
        return 2;
    }
    static unsigned char table[PATTERNS];
    char line[512];
    long count = 0;
    while (fgets(line, sizeof line, in) != NULL) {
        unsigned long p;
        int par;
        char sol[300];
        if (sscanf(line, "%lu %d %299s", &p, &par, sol) != 3 || (long)p != count || p >= PATTERNS || par < 0 || par > 254) {
            fprintf(stderr, "cube_data: %s line %ld is not pattern %ld's par: %s", from, count + 1, count, line);
            return 2;
        }
        // The solution is the census's; it must roll to its pattern in par rolls.
        int k = par == 0 ? 0 : (int)strlen(sol);
        if (k != par || !cube_check(N, 0, FACE_UP, (uint32_t)p, par == 0 ? "" : sol, k)) {
            fprintf(stderr, "cube_data: pattern %lu's solution %s does not roll to it in %d\n", p, sol, par);
            return 2;
        }
        table[p] = (unsigned char)par;
        count++;
    }
    fclose(in);
    if (count != PATTERNS) {
        fprintf(stderr, "cube_data: %s has %ld patterns, not %d\n", from, count, PATTERNS);
        return 2;
    }
    FILE *out = fopen(to, "wb");
    if (out == NULL || fwrite(table, 1, PATTERNS, out) != PATTERNS || fclose(out) != 0) {
        fprintf(stderr, "cube_data: cannot write %s\n", to);
        return 2;
    }
    printf("cube_data: %d pars, %ld held to their solutions\n", PATTERNS, count);
    return 0;
}

static uint32_t seed = 12345;

static uint32_t next_random(void) {
    seed = seed * 1103515245u + 12345u;
    return seed >> 8;
}

// One program's line: the program, then each prefix's frame by cube_play.
static void write_run(FILE *out, const char *program) {
    int k = (int)strlen(program);
    fputs(k == 0 ? "-" : program, out);
    for (int i = 1; i <= k; i++) {
        int cell, halt_at;
        CubeFace face;
        int64_t lit = cube_play(N, 0, FACE_UP, program, i, &cell, &face, &halt_at);
        if (halt_at >= 0) {
            fprintf(out, " halt@%d", halt_at);
            break;
        }
        fprintf(out, " %d.%d.%lld", cell, (int)face, (long long)lit);
    }
    fputc('\n', out);
}

static int runs(const char *to) {
    FILE *out = fopen(to, "w");
    if (out == NULL) {
        fprintf(stderr, "cube_data: cannot write %s\n", to);
        return 2;
    }
    static const char *LEVELS[] = {"", "ESWN", "ESENSNWS", "ESSENNWWESESESNNWW", "EEESWNWSENWSWNESWESWESWNESENWNESESWNESWNEWNEWNE"};
    int written = 0;
    for (size_t i = 0; i < sizeof LEVELS / sizeof LEVELS[0]; i++, written++) write_run(out, LEVELS[i]);
    char program[MAX_ROLLS + 1];
    for (; written < SAMPLES; written++) {
        int k = 1 + (int)(next_random() % MAX_ROLLS);
        int cell = 0;
        for (int i = 0; i < k; i++) {
            int r = (int)(next_random() % CUBE_ROLLS);
            // One roll in 40 may leave the board; the rest are redrawn until
            // they stay on it.
            if (next_random() % 40 != 0) {
                while (cube_cell_after(N, cell, (CubeRoll)r) < 0) r = (int)(next_random() % CUBE_ROLLS);
            }
            program[i] = cube_roll_letter((CubeRoll)r);
            int to_cell = cube_cell_after(N, cell, (CubeRoll)r);
            if (to_cell >= 0) cell = to_cell;
        }
        program[k] = '\0';
        write_run(out, program);
    }
    if (fclose(out) != 0) return 2;
    printf("cube_data: %d programs' runs\n", written);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "pars") == 0) return pars(argv[2], argv[3]);
    if (argc == 3 && strcmp(argv[1], "runs") == 0) return runs(argv[2]);
    fprintf(stderr, "usage: cube_data pars PARS BIN | cube_data runs OUT\n");
    return 2;
}
