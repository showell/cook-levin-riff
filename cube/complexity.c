// complexity.c: the computational checks behind cube/COMPLEXITY.md.
//
//   complexity N PARS     N 3 or 4, PARS the file `census N PARS` writes:
//                         the Hamiltonian lemma on every lit set, and on
//                         3 x 3 the state graph and the column tours, on
//                         4 x 4 the sets with one dark tile and the
//                         reduction from Hamiltonian cycle, run on every
//                         grid graph in a 4 x 4 box
//   complexity graph N    the state graph alone, N 3 or 4 (4 is 96 searches
//                         of 6.3 million states: minutes, not in the build)
//
// The lemma: for a non-empty set T, par(T) = 3|T| - 1, the floor, exactly
// when the grid graph induced on T has a Hamiltonian path that starts at c1
// or a3. Each set's path is found here by a plain depth-first search over
// paths in T, with an adjacency of its own (no cube rules), and compared
// with the census's par. Any disagreement is printed and exits 1.
//
// The state graph: a state is (cell, painted face, pattern), an edge a roll
// that stays on the board (a roll off it halts the machine, and leads to no
// state). The checks: every state's in-degree is its out-degree, every
// state reaches every state, and each state's eccentricity, the most rolls
// a shortest path from it needs. Flipping a fixed set of tiles in every
// state's pattern maps the graph onto itself (a roll's flip is an XOR that
// depends only on the cell and face), so a state's eccentricity is that of
// its cell and face with nothing lit: n^2 * 6 searches cover every state.

#include "cube.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_TILES = 16 };

static int popcount(uint32_t x) {
    int k = 0;
    for (; x != 0; x &= x - 1) k++;
    return k;
}

static int on_a1_colour(int n, int cell) { return (cell / n + cell % n) % 2 == 0; }

static void cell_name(int n, int cell, char out[4]) {
    out[0] = (char)('a' + cell % n);
    out[1] = (char)('1' + cell / n);
    out[2] = '\0';
}

// The grid neighbours of `cell`, its own adjacency: gives their count.
static int neighbours(int n, int cell, int out[4]) {
    int row = cell / n, col = cell % n, k = 0;
    if (row > 0) out[k++] = cell - n;
    if (row < n - 1) out[k++] = cell + n;
    if (col > 0) out[k++] = cell - 1;
    if (col < n - 1) out[k++] = cell + 1;
    return k;
}

// A path in T from `v`, having visited `seen`, that visits all of T; the
// path found is left in path[depth..].
static int path_covers(int n, uint32_t t, int v, uint32_t seen, int *path, int depth) {
    path[depth] = v;
    if (seen == t) return 1;
    int next[4];
    int k = neighbours(n, v, next);
    for (int i = 0; i < k; i++) {
        int w = next[i];
        uint32_t bit = 1u << w;
        if ((t & bit) && !(seen & bit) && path_covers(n, t, w, seen | bit, path, depth + 1)) return 1;
    }
    return 0;
}

static int found_path[MAX_TILES];

static int ham_from(int n, uint32_t t, int s) { return (t >> s & 1) && path_covers(n, t, s, 1u << s, found_path, 0); }

// The lemma's "if": the program a Hamiltonian path from c1 or a3 gives,
// EE or SS to land on its first cell, then three rolls a step: a roll r
// across the step, the step, and r back. Gives its length.
static int program_of(int n, const int *path, int cells, char *out) {
    int k = 0;
    const char *first = path[0] == 2 ? "EE" : "SS";
    out[k++] = first[0];
    out[k++] = first[1];
    for (int i = 1; i < cells; i++) {
        int v = path[i - 1], w = path[i];
        int dr = w / n - v / n, dc = w % n - v % n;
        char step = dr == 1 ? 'S' : dr == -1 ? 'N' : dc == 1 ? 'E' : 'W';
        // Across the step: N or S for a step E or W, else E or W, whichever
        // keeps the cube on the board.
        char across, back;
        if (dr == 0) {
            int up_ok = v / n > 0;
            across = up_ok ? 'N' : 'S';
            back = up_ok ? 'S' : 'N';
        } else {
            int left_ok = v % n > 0;
            across = left_ok ? 'W' : 'E';
            back = left_ok ? 'E' : 'W';
        }
        out[k++] = across;
        out[k++] = step;
        out[k++] = back;
    }
    out[k] = '\0';
    return k;
}

// The program of the path just found rolls to exactly T in 3|T| - 1.
static int program_checks(int n, uint32_t t) {
    char prog[3 * MAX_TILES];
    int cells = popcount(t);
    int k = program_of(n, found_path, cells, prog);
    return k == 3 * cells - 1 && cube_check(n, 0, FACE_UP, t, prog, k);
}

// Any Hamiltonian path in T, from any cell.
static int ham_any(int n, uint32_t t) {
    for (int s = 0; s < n * n; s++) {
        if (ham_from(n, t, s)) return 1;
    }
    return 0;
}

// The census's pars, read from PARS: -1 for an unreachable pattern.
static int *read_pars(int n, const char *path) {
    uint32_t patterns = 1u << (n * n);
    int *par = malloc(patterns * sizeof(int));
    FILE *f = fopen(path, "r");
    if (par == NULL || f == NULL) {
        fprintf(stderr, "complexity: cannot read %s\n", path);
        exit(2);
    }
    char word[16], sol[256];
    for (uint32_t p = 0; p < patterns; p++) {
        unsigned q;
        if (fscanf(f, "%u %15s", &q, word) != 2 || q != p) {
            fprintf(stderr, "complexity: %s: line %u is not pattern %u\n", path, p + 1, p);
            exit(2);
        }
        par[p] = strcmp(word, "-") == 0 ? -1 : atoi(word);
        if (par[p] >= 0 && fscanf(f, "%255s", sol) != 1) {
            fprintf(stderr, "complexity: %s: pattern %u has no solution\n", path, p);
            exit(2);
        }
    }
    fclose(f);
    return par;
}

static void print_grid(int n, uint32_t t) {
    for (int r = 0; r < n; r++) {
        printf("    ");
        for (int c = 0; c < n; c++) putchar(t >> (r * n + c) & 1 ? '#' : '.');
        putchar('\n');
    }
}

// The lemma on every non-empty set; gives the number of disagreements.
static int lemma(int n, const int *par) {
    uint32_t patterns = 1u << (n * n);
    int c1 = 2, a3 = 2 * n;
    long floor_sets = 0, ham_sets = 0, both = 0, c1_only = 0, a3_only = 0, other_start = 0, rolled = 0, bad = 0;
    for (uint32_t t = 1; t < patterns; t++) {
        int meets = par[t] == 3 * popcount(t) - 1;
        int from_a3 = ham_from(n, t, a3);
        if (from_a3 && program_checks(n, t)) rolled++;
        int from_c1 = ham_from(n, t, c1);
        if (from_c1 && program_checks(n, t)) rolled++;
        int ham = from_c1 || from_a3;
        floor_sets += meets;
        ham_sets += ham;
        if (from_c1 && from_a3) both++;
        else if (from_c1) c1_only++;
        else if (from_a3) a3_only++;
        else if (ham_any(n, t)) other_start++;
        if (meets != ham) {
            if (bad < 5) {
                printf("COUNTEREXAMPLE: par %d, floor %d, Hamiltonian path from c1 or a3: %s\n", par[t], 3 * popcount(t) - 1, ham ? "yes" : "no");
                print_grid(n, t);
            }
            bad++;
        }
    }
    printf("cube %dx%d: the Hamiltonian lemma on the %u non-empty sets\n", n, n, patterns - 1);
    printf("  par at the floor 3|T| - 1: %ld sets\n", floor_sets);
    printf("  a Hamiltonian path from c1 or a3: %ld sets (from both %ld, c1 only %ld, a3 only %ld)\n", ham_sets, both, c1_only, a3_only);
    printf("  each path's program (EE or SS, then three rolls a step) rolled to its set in 3|T| - 1: %ld of %ld paths\n", rolled, 2 * both + c1_only + a3_only);
    printf("  a Hamiltonian path, but none from c1 or a3: %ld sets, every one over the floor\n", other_start);
    printf("  the two agree on %ld of %u sets\n", (long)(patterns - 1) - bad, patterns - 1);
    printf("  the empty set: par %d, the formula's -1 plus one; the lemma is for non-empty sets\n", par[0]);
    if (rolled != 2 * both + c1_only + a3_only) bad++;
    return (int)bad;
}

// Every tile lit but one, on 4 x 4; gives the number of rows that break the
// colour rule.
static int one_dark(int n, const int *par) {
    uint32_t all = (1u << (n * n)) - 1;
    int tiles = n * n, bad = 0;
    printf("\nevery tile lit but one: floor %d\n", 3 * (tiles - 1) - 1);
    printf("dark  colour  a1-colour cells lit  par  path from c1 or a3\n");
    for (int x = 0; x < tiles; x++) {
        uint32_t t = all & ~(1u << x);
        int same = 0;
        for (int c = 0; c < tiles; c++) {
            if ((t >> c & 1) && on_a1_colour(n, c)) same++;
        }
        int ham = ham_from(n, t, 2) || ham_from(n, t, 2 * n);
        // A path on 15 cells alternates colours, so it starts on the colour
        // with 8 of them; c1 and a3 are a1's colour.
        int want = same == (tiles) / 2 ? 3 * (tiles - 1) - 1 : 3 * (tiles - 1);
        char name[4];
        cell_name(n, x, name);
        printf("%-4s  %-6s  %19d  %3d  %s\n", name, on_a1_colour(n, x) ? "a1's" : "other", same, par[t], ham ? "yes" : "no");
        if (par[t] != want || ham != (same == tiles / 2)) bad++;
    }
    printf("par 44 exactly where 8 lit cells are a1's colour, else 45: %s\n", bad == 0 ? "yes" : "NO");
    return bad;
}

// Every tile lit at the floor, 3n^2 - 1, on n x n for n from 3 to 5: the
// tour c1, b1, a1, down column a, up column b to b2, down column c from c2,
// up column d, down column e, and so on, rolled; gives the failures.
static int tours(void) {
    int bad = 0;
    printf("\nevery tile lit, by the column tour from c1:\n");
    for (int n = 3; n <= CUBE_MAX_N; n++) {
        int path[CUBE_MAX_N * CUBE_MAX_N], k = 0;
        path[k++] = 2;
        path[k++] = 1;
        for (int r = 0; r < n; r++) path[k++] = r * n;
        for (int r = n - 1; r >= 1; r--) path[k++] = r * n + 1;
        for (int r = 1; r < n; r++) path[k++] = r * n + 2;
        for (int c = 3; c < n; c++) {
            for (int i = 0; i < n; i++) path[k++] = (c % 2 == 1 ? n - 1 - i : i) * n + c;
        }
        char prog[3 * CUBE_MAX_N * CUBE_MAX_N];
        int rolls = program_of(n, path, k, prog);
        uint32_t all = (1u << (n * n)) - 1;
        int ok = k == n * n && rolls == 3 * n * n - 1 && cube_check(n, 0, FACE_UP, all, prog, rolls);
        printf("  %dx%d: %d rolls, the floor %d: %s\n", n, n, rolls, 3 * n * n - 1, ok ? "every tile lit" : "FAILS");
        if (!ok) bad++;
    }
    return bad;
}

// The reduction of COMPLEXITY.md on every grid graph G in a 4 x 4 box,
// cells (row, col) on a canvas of CANVAS x CANVAS, a bit a cell.
enum { CANVAS = 8 };

static int cv_covers(uint64_t t, int v, uint64_t seen, int close) {
    if (seen == t) {
        if (close < 0) return 1;
        int dr = v / CANVAS - close / CANVAS, dc = v % CANVAS - close % CANVAS;
        return dr * dr + dc * dc == 1;
    }
    int row = v / CANVAS, col = v % CANVAS;
    int next[4], k = 0;
    if (row > 0) next[k++] = v - CANVAS;
    if (row < CANVAS - 1) next[k++] = v + CANVAS;
    if (col > 0) next[k++] = v - 1;
    if (col < CANVAS - 1) next[k++] = v + 1;
    for (int i = 0; i < k; i++) {
        uint64_t bit = (uint64_t)1 << next[i];
        if ((t & bit) && !(seen & bit) && cv_covers(t, next[i], seen | bit, close)) return 1;
    }
    return 0;
}

static int cv_count(uint64_t t) {
    int k = 0;
    for (; t != 0; t &= t - 1) k++;
    return k;
}

static int cv_connected(uint64_t t) {
    if (t == 0) return 0;
    uint64_t seen = t & -t, grow = seen;
    while (grow != 0) {
        uint64_t spread = (seen << CANVAS) | (seen >> CANVAS) | ((seen << 1) & ~0x0101010101010101ull) | ((seen >> 1) & ~0x8080808080808080ull);
        grow = spread & t & ~seen;
        seen |= grow;
    }
    return seen == t;
}

static int cv_degree(uint64_t t, int v) {
    int k = 0, row = v / CANVAS, col = v % CANVAS;
    if (row > 0 && (t >> (v - CANVAS) & 1)) k++;
    if (row < CANVAS - 1 && (t >> (v + CANVAS) & 1)) k++;
    if (col > 0 && (t >> (v - 1) & 1)) k++;
    if (col < CANVAS - 1 && (t >> (v + 1) & 1)) k++;
    return k;
}

static int reduction(void) {
    long kept = 0, cycles = 0, agree = 0, trivial = 0, trivial_ok = 0, tips_ok = 0;
    int largest = 0;
    for (uint32_t g = 0; g < 1u << 16; g++) {
        // G on the canvas at rows and columns 0..3.
        uint64_t gv = 0;
        int r0 = -1, c0 = -1, cmin = 4;
        for (int cell = 0; cell < 16; cell++) {
            if (!(g >> cell & 1)) continue;
            int r = cell / 4, c = cell % 4;
            gv |= (uint64_t)1 << (r * CANVAS + c);
            if (r0 < 0) r0 = r, c0 = c;
            if (c < cmin) cmin = c;
        }
        // A Hamiltonian cycle needs 3 cells, one piece, and u = the top
        // row's leftmost cell with both its neighbours, east and south.
        int u = r0 * CANVAS + c0;
        int has_cycle = 0;
        if (popcount(g) >= 3) has_cycle = cv_covers(gv, u, (uint64_t)1 << u, u);
        int x_in = c0 < 3 && (gv >> (u + 1) & 1), y_in = r0 < 3 && (gv >> (u + CANVAS) & 1);
        if (popcount(g) < 3 || !cv_connected(gv) || !x_in || !y_in) {
            trivial++;
            if (!has_cycle) trivial_ok++;
            continue;
        }
        kept++;
        cycles += has_cycle;
        // T: G, then f, a and b above u and x, and the tail from b up two
        // and west along its row to column m, its tip; moved so the tip is
        // c1, row 0 and column 2.
        int m = (cmin < c0 - 1 ? cmin : c0 - 1) + 2;
        int dr = 3 - r0, dc = 2 - m;
        uint64_t t = 0;
#define PUT(r, c) (t |= (uint64_t)1 << (((r) + dr) * CANVAS + (c) + dc))
        for (int cell = 0; cell < 16; cell++) {
            if (g >> cell & 1) PUT(cell / 4, cell % 4);
        }
        PUT(r0 - 1, c0 - 1);
        PUT(r0 - 1, c0);
        PUT(r0 - 1, c0 + 1);
        PUT(r0 - 2, c0 + 1);
        for (int c = c0 + 1; c >= m; c--) PUT(r0 - 3, c);
#undef PUT
        int n = 3;
        for (int v = 0; v < CANVAS * CANVAS; v++) {
            if (t >> v & 1) {
                if (v / CANVAS + 1 > n) n = v / CANVAS + 1;
                if (v % CANVAS + 1 > n) n = v % CANVAS + 1;
            }
        }
        if (n > largest) largest = n;
        int tip = 2, f = (r0 - 1 + dr) * CANVAS + c0 - 1 + dc;
        if ((t >> tip & 1) && cv_degree(t, tip) == 1 && cv_degree(t, f) == 1) tips_ok++;
        int a3 = 2 * CANVAS;
        int path = cv_covers(t, tip, (uint64_t)1 << tip, -1) || ((t >> a3 & 1) && cv_covers(t, a3, (uint64_t)1 << a3, -1));
        if (path == has_cycle && cv_count(t) == popcount(g) + 5 + (c0 + 1 - m)) agree++;
    }
    printf("\nthe reduction on every grid graph G in a 4 x 4 box:\n");
    printf("  turned away (under 3 cells, in pieces, or u lacking a neighbour): %ld, none with a Hamiltonian cycle: %s\n", trivial, trivial_ok == trivial ? "yes" : "NO");
    printf("  reduced: %ld, %ld with a Hamiltonian cycle, on boards up to %d x %d\n", kept, cycles, largest, largest);
    printf("  both tips of T of degree 1, one at c1: %ld of %ld\n", tips_ok, kept);
    printf("  a Hamiltonian cycle in G exactly when T has a path from c1 or a3: %ld of %ld\n", agree, kept);
    return (trivial_ok != trivial) + (tips_ok != kept) + (agree != kept);
}

// The state graph of an n x n board; gives the number of failed checks.
static int graph(int n) {
    int tiles = n * n;
    uint32_t patterns = 1u << tiles;
    uint32_t states = (uint32_t)tiles * CUBE_FACES * patterns;
    uint8_t *in = calloc(states, 1);
    uint8_t *level = malloc(states);
    uint32_t *queue = malloc((size_t)states * sizeof(uint32_t));
    if (in == NULL || level == NULL || queue == NULL) {
        fprintf(stderr, "complexity: out of memory\n");
        exit(2);
    }
    int bad = 0;

    // In-degree against out-degree: each roll has one state it came from.
    uint32_t edges = 0;
    for (uint32_t s = 0; s < states; s++) {
        int cell = (int)(s >> tiles) / CUBE_FACES;
        CubeFace face = (CubeFace)((int)(s >> tiles) % CUBE_FACES);
        uint32_t p = s & (patterns - 1);
        for (int r = 0; r < CUBE_ROLLS; r++) {
            int to = cube_cell_after(n, cell, (CubeRoll)r);
            if (to < 0) continue;
            CubeFace f = cube_face_after(face, (CubeRoll)r);
            uint32_t q = f == FACE_DOWN ? p ^ (1u << to) : p;
            in[((uint32_t)(to * CUBE_FACES + (int)f) << tiles) | q]++;
            edges++;
        }
    }
    uint32_t balanced = 0;
    for (uint32_t s = 0; s < states; s++) {
        int cell = (int)(s >> tiles) / CUBE_FACES;
        int nb[4];
        if (in[s] == neighbours(n, cell, nb)) balanced++;
    }
    printf("\ncube %dx%d: the state graph, %u states and %u rolls that stay on the board\n", n, n, states, edges);
    printf("  in-degree equals out-degree: %u of %u states\n", balanced, states);
    if (balanced != states) bad++;

    // A search from each cell and face with nothing lit.
    int diameter = 0, ecc[MAX_TILES][CUBE_FACES];
    uint32_t all_reach = 0;
    for (int cell = 0; cell < tiles; cell++) {
        for (int face = 0; face < CUBE_FACES; face++) {
            memset(level, 255, states);
            uint32_t s0 = (uint32_t)(cell * CUBE_FACES + face) << tiles;
            level[s0] = 0;
            queue[0] = s0;
            uint32_t found = 1;
            int deepest = 0;
            for (uint32_t head = 0; head < found; head++) {
                uint32_t s = queue[head];
                int c = (int)(s >> tiles) / CUBE_FACES;
                CubeFace fc = (CubeFace)((int)(s >> tiles) % CUBE_FACES);
                uint32_t p = s & (patterns - 1);
                for (int r = 0; r < CUBE_ROLLS; r++) {
                    int to = cube_cell_after(n, c, (CubeRoll)r);
                    if (to < 0) continue;
                    CubeFace f = cube_face_after(fc, (CubeRoll)r);
                    uint32_t q = f == FACE_DOWN ? p ^ (1u << to) : p;
                    uint32_t t = ((uint32_t)(to * CUBE_FACES + (int)f) << tiles) | q;
                    if (level[t] != 255) continue;
                    level[t] = (uint8_t)(level[s] + 1);
                    if (level[t] > deepest) deepest = level[t];
                    queue[found++] = t;
                }
            }
            ecc[cell][face] = deepest;
            if (deepest > diameter) diameter = deepest;
            if (found == states) all_reach++;
        }
    }
    printf("  every state reached from each cell and face with nothing lit: %u of %d searches\n", all_reach, tiles * CUBE_FACES);
    printf("  so the graph is strongly connected (flipping a fixed set in every pattern maps it onto itself)\n");
    if (all_reach != (uint32_t)(tiles * CUBE_FACES)) bad++;
    static const char *NAMES[CUBE_FACES] = {"up", "down", "north", "south", "east", "west"};
    printf("  eccentricity by start cell, one grid a painted face:\n");
    for (int face = 0; face < CUBE_FACES; face++) {
        printf("    %-5s", NAMES[face]);
        for (int r = 0; r < n; r++) {
            printf(" ");
            for (int c = 0; c < n; c++) printf(" %2d", ecc[r * n + c][face]);
        }
        printf("\n");
    }
    printf("  eccentricity of the start, a1 face up: %d\n", ecc[0][FACE_UP]);
    printf("  the diameter D%d: %d\n", n, diameter);
    free(in);
    free(level);
    free(queue);
    return bad;
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "graph") == 0) {
        int n = atoi(argv[2]);
        if (n < 2 || n > 4) {
            fprintf(stderr, "complexity: N must be 2, 3 or 4\n");
            return 2;
        }
        return graph(n) == 0 ? 0 : 1;
    }
    if (argc != 3) {
        fprintf(stderr, "usage: complexity N PARS | complexity graph N\n");
        return 2;
    }
    int n = atoi(argv[1]);
    if (n != 3 && n != 4) {
        fprintf(stderr, "complexity: N must be 3 or 4, not %s\n", argv[1]);
        return 2;
    }
    int *par = read_pars(n, argv[2]);
    int bad = lemma(n, par);
    if (n == 3) bad += graph(n) + tours();
    if (n == 4) bad += one_dark(n, par) + reduction();
    free(par);
    if (fflush(stdout) != 0) return 1;
    return bad == 0 ? 0 : 1;
}
