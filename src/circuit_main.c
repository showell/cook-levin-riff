// circuit_main.c: the circuit chapters' drawings and tables, from the
// netlist of circuit.c.
//
//   circuit_main            the blocks of "Parts for a small computer",
//                           "The machine as a circuit" and "The circuit, in
//                           full", a blank line apart, each a ```dot fence or
//                           a Markdown table
//   circuit_main svg DIR    the schematics of the parts drawn at gate level,
//                           one SVG file each in DIR: nada, zero, decr,
//                           mod2, mux2, hold and latches
//   circuit_main equations  the decider as Boolean equations, one a gate
//                           (net_equations), which "The circuit as
//                           equations" shows whole and
//                           python/equations.py evaluates on its own
//
// The tables are the simulator's answers on every input they list. Each
// part's innards are drawn once, as a schematic; above that, a part is a
// box, and the box drawings (a mux4 of three mux2s, a stepper, a core, the
// decider) come from the links the netlist's composers record, so they show
// the wiring the simulator runs. book/book.shale holds the book's source to
// every block byte for byte, and the book embeds the schematics as this
// writes them.
// A unity build, as main.c is.

#include "vm.c"
#include "circuit.c"

#include <string.h>

static Net NET;
static int VALUES[CIRCUIT_MAX_WIRES];

static int port(const char *part, const char *name) { return VALUES[net_port_wire(&NET, part, name)]; }

static int has_port(const char *part, const char *name) {
    for (int i = 0; i < NET.num_ports; i++) {
        if (strcmp(NET.ports[i].part, part) == 0 && strcmp(NET.ports[i].name, name) == 0) return 1;
    }
    return 0;
}

typedef MachineOut (*Build)(Net *, const char *, int, int);
static const Build BUILD[4] = {part_nada, part_zero, part_decr, part_mod2};

static void machine_net(int op) {
    net_init(&NET);
    int hb = net_input(&NET, "hb"), lb = net_input(&NET, "lb");
    BUILD[op](&NET, MACHINES[op], hb, lb);
}

// The inputs made in the order in 0, s, in 1, so the drawing's wires
// cross least.
static void mux2_net(void) {
    net_init(&NET);
    int a = net_input(&NET, "in 0"), s = net_input(&NET, "s"), b = net_input(&NET, "in 1");
    part_mux2(&NET, "mux2", s, a, b);
}

static void mux4_net(void) {
    net_init(&NET);
    int s_hb = net_input(&NET, "s_hb"), s_lb = net_input(&NET, "s_lb");
    int d[4];
    char name[8];
    for (int k = 0; k < 4; k++) {
        snprintf(name, sizeof name, "in %d", k);
        d[k] = net_input(&NET, name);
    }
    part_mux4(&NET, "mux4", s_hb, s_lb, d);
}

// The inputs made in the order op_hb, halted, op_lb, so the drawing's
// wires cross least.
static void hold_net(void) {
    net_init(&NET);
    int op_hb = net_input(&NET, "op_hb"), h = net_input(&NET, "halted"), op_lb = net_input(&NET, "op_lb");
    int sel[2];
    part_hold(&NET, "hold", op_hb, op_lb, h, sel);
}

static void latches_net(void) {
    net_init(&NET);
    int halted = net_input(&NET, "halted"), halts = net_input(&NET, "halts");
    int accepted = net_input(&NET, "accepted"), accepts = net_input(&NET, "accepts");
    part_latch(&NET, "latches", "halted'", halted, halts);
    part_latch(&NET, "latches", "accepted'", accepted, accepts);
}

// The four machines on every value of AX: each cell is hb' and lb', and a
// flag when it is on.
static void machines_table(void) {
    printf("| AX | hb lb | `nada` | `zero` | `decr` | `mod2` |\n|---|---|---|---|---|---|\n");
    for (int x = 0; x < 4; x++) {
        printf("| %d | %d %d |", x, x / 2, x % 2);
        for (int op = 0; op < NUM_OPS; op++) {
            machine_net(op);
            int in[2] = {x / 2, x % 2};
            net_eval(&NET, in, VALUES);
            const char *m = MACHINES[op];
            printf(" %d %d", port(m, "hb'"), port(m, "lb'"));
            if (has_port(m, "accepts") && port(m, "accepts")) printf(", accepts");
            if (has_port(m, "halts") && port(m, "halts")) printf(", halts");
            printf(" |");
        }
        printf("\n");
    }
    printf("\n");
}

static void mux2_table(void) {
    mux2_net();
    printf("| s | in 0 | in 1 | out |\n|---|---|---|---|\n");
    for (int v = 0; v < 8; v++) {
        int s = v >> 2 & 1, a = v >> 1 & 1, b = v & 1;
        int in[3] = {a, s, b};
        net_eval(&NET, in, VALUES);
        printf("| %d | %d | %d | %d |\n", s, a, b, port("mux2", "out"));
    }
    printf("\n");
}

static void hold_table(void) {
    hold_net();
    printf("| halted | op_hb op_lb | sel_hb sel_lb |\n|---|---|---|\n");
    for (int v = 0; v < 8; v++) {
        int op_hb = v >> 1 & 1, op_lb = v & 1, halted = v >> 2 & 1;
        int in[3] = {op_hb, halted, op_lb};
        net_eval(&NET, in, VALUES);
        printf("| %d | %d %d | %d %d |\n", halted, op_hb, op_lb, port("hold", "sel_hb"), port("hold", "sel_lb"));
    }
    printf("\n");
}

static void latch_table(void) {
    latches_net();
    printf("| accepted | accepts | accepted' |\n|---|---|---|\n");
    for (int v = 0; v < 4; v++) {
        int in[4] = {0, 0, v >> 1, v & 1};
        net_eval(&NET, in, VALUES);
        printf("| %d | %d | %d |\n", v >> 1, v & 1, port("latches", "accepted'"));
    }
    printf("\n");
}

static void boxes_block(const char *scope, const char *rankdir) {
    printf("```dot\n");
    net_boxes_dot(&NET, scope, rankdir, stdout);
    printf("```\n\n");
}

static const char *const STEPPER_PARTS[] = {"nada", "zero", "decr", "mod2", "hold", "hb mux", "lb mux",
                                            "accepts mux", "halts mux", "latches"};
enum { NUM_STEPPER_PARTS = 10 };

// Each box of a stepper: how many wires go in and come out, and how many
// gates are inside, read from the stepper's links and ports.
static void vocabulary(void) {
    circuit_stepper(&NET);
    printf("| box | wires in | wires out | gates |\n|---|---|---|---|\n");
    for (int i = 0; i < NUM_STEPPER_PARTS; i++) {
        int in = 0, out = 0;
        for (int k = 0; k < NET.num_links; k++) {
            if (strcmp(NET.links[k].scope, "") == 0 && strcmp(NET.links[k].to, STEPPER_PARTS[i]) == 0) in++;
        }
        for (int k = 0; k < NET.num_ports; k++) {
            if (strcmp(NET.ports[k].part, STEPPER_PARTS[i]) == 0) out++;
        }
        GateCount c = net_count(&NET, STEPPER_PARTS[i]);
        printf("| %s | %d | %d | %d |\n", STEPPER_PARTS[i], in, out, c.nots + c.ands + c.ors);
    }
    printf("\n");
}

// A count as the book writes one, a comma before each group of three.
static void count(int n) {
    if (n >= 1000) printf("%d,%03d", n / 1000, n % 1000);
    else printf("%d", n);
}

static void count_row(const char *label, GateCount c) {
    printf("| %s |", label);
    int all[4] = {c.nots, c.ands, c.ors, c.nots + c.ands + c.ors};
    for (int k = 0; k < 4; k++) {
        printf(" ");
        count(all[k]);
        printf(" |");
    }
    printf("\n");
}

static void counts(void) {
    circuit_stepper(&NET);
    printf("| part | NOT | AND | OR | gates |\n|---|---|---|---|---|\n");
    for (int i = 0; i < NUM_STEPPER_PARTS; i++) count_row(STEPPER_PARTS[i], net_count(&NET, STEPPER_PARTS[i]));
    count_row("**the stepper**", net_count(&NET, ""));
    circuit_core(&NET);
    count_row("**a core**, six steppers", net_count(&NET, ""));
    circuit_decides(&NET);
    count_row("**the decider**, four cores and the comparators", net_count(&NET, ""));
    printf("\n");
}

static void blocks(void) {
    machines_table();
    mux2_table();
    mux4_net();
    boxes_block("mux4", "LR");
    hold_table();
    latch_table();
    vocabulary();
    circuit_stepper(&NET);
    boxes_block("", "TB");
    counts();
    circuit_core(&NET);
    boxes_block("", "TB");
    circuit_decides(&NET);
    boxes_block("", "LR");
    circuit_stepper(&NET);
    printf("```text\n");
    net_equations(&NET, stdout);
    printf("```\n\n");
}

static void write_svg(const char *dir, const char *name, const char *part) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s.svg", dir, name);
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        fprintf(stderr, "circuit_main: can't write %s\n", path);
        exit(1);
    }
    net_svg(&NET, part, f);
    if (fclose(f) != 0) {
        fprintf(stderr, "circuit_main: can't write %s\n", path);
        exit(1);
    }
}

static void schematics(const char *dir) {
    for (int op = 0; op < NUM_OPS; op++) {
        machine_net(op);
        write_svg(dir, MACHINES[op], MACHINES[op]);
    }
    mux2_net();
    write_svg(dir, "mux2", "mux2");
    hold_net();
    write_svg(dir, "hold", "hold");
    latches_net();
    write_svg(dir, "latches", "latches");
}

int main(int argc, char **argv) {
    if (argc == 1) {
        blocks();
    } else if (argc == 3 && strcmp(argv[1], "svg") == 0) {
        schematics(argv[2]);
    } else if (argc == 2 && strcmp(argv[1], "equations") == 0) {
        circuit_decides(&NET);
        net_equations(&NET, stdout);
    } else {
        fprintf(stderr, "usage: circuit_main [svg DIR | equations]\n");
        return 2;
    }
    return fflush(stdout) == 0 ? 0 : 1;
}
