// gates_main.c: the cube's one-roll circuit and its unrolled CNF on the
// command line (gates.h). A unity build, as cube_sat.c is.
//
//   cube_gates count N            the one-roll circuit's gates, part by part
//   cube_gates cnf N TARGET K     the CNF of k rolls by Tseitin's encoding, on
//                                 stdout: an N x N grid, the cube at a1 with the
//                                 face up, TARGET the pattern to end with (tile
//                                 r * N + c is bit r * N + c), as cube_sat cnf
//   cube_gates size N TARGET K    that CNF's variables and clauses, one line
//   cube_gates dot N              the five parts as boxes, Graphviz source

#include "../src/circuit.c"
#include "cube.c"
#include "gates.c"

#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fprintf(stderr, "usage: cube_gates count N | cube_gates cnf N TARGET K | cube_gates size N TARGET K | cube_gates dot N\n");
    return 2;
}

static Net NET;

int main(int argc, char **argv) {
    if (argc < 3) return usage();
    int n = atoi(argv[2]);
    if (n < 2 || n > CUBE_MAX_N) {
        fprintf(stderr, "cube_gates: N must be 2 to %d, not %s\n", CUBE_MAX_N, argv[2]);
        return 2;
    }
    if (strcmp(argv[1], "count") == 0 && argc == 3) {
        cube_roll_circuit(&NET, n);
        printf("part    NOT  AND   OR  gates\n");
        for (int p = 0; p < CUBE_PARTS; p++) {
            GateCount g = net_count(&NET, CUBE_PART_NAMES[p]);
            printf("%-6s %4d %4d %4d %6d\n", CUBE_PART_NAMES[p], g.nots, g.ands, g.ors, g.nots + g.ands + g.ors);
        }
        GateCount all = net_count(&NET, "");
        printf("%-6s %4d %4d %4d %6d\n", "all", all.nots, all.ands, all.ors, all.nots + all.ands + all.ors);
        return fflush(stdout) == 0 ? 0 : 1;
    }
    if (strcmp(argv[1], "dot") == 0 && argc == 3) {
        cube_roll_circuit(&NET, n);
        net_boxes_dot(&NET, "", "LR", stdout);
        return fflush(stdout) == 0 ? 0 : 1;
    }
    if ((strcmp(argv[1], "cnf") != 0 && strcmp(argv[1], "size") != 0) || argc != 5) return usage();
    uint32_t target = (uint32_t)strtoul(argv[3], NULL, 10);
    if (n * n < 32 && target >> (n * n) != 0) {
        fprintf(stderr, "cube_gates: pattern %s has a tile past %d\n", argv[3], n * n);
        return 2;
    }
    int k = atoi(argv[4]);
    if (k < 1 || k > 1000) {
        fprintf(stderr, "cube_gates: K must be 1 to 1000, not %s\n", argv[4]);
        return 2;
    }
    CubeChain c = cube_chain(n, k, target);
    int vars;
    long clauses;
    cube_chain_dimacs(&c, strcmp(argv[1], "cnf") == 0 ? stdout : NULL, &vars, &clauses);
    if (strcmp(argv[1], "size") == 0) printf("%d variables, %ld clauses\n", vars, clauses);
    cube_chain_free(&c);
    return fflush(stdout) == 0 ? 0 : 1;
}
