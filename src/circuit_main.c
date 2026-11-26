// circuit_main.c: the circuit chapters' drawings and tables, from the
// netlist of circuit.c.
//
//   circuit_main
//
// It prints, in order, the blocks of "Parts for a small computer", "The
// machine as a circuit" and "The circuit, in full": each part's drawing at
// gate level and its table on every input, the parts as boxes, a stepper
// drawn as its boxes, the gate counts, a core drawn as six stepper boxes,
// and the decider drawn as four core boxes, its comparators and its AND.
// The box drawings come from the links the netlist's composers record, so
// they show the wiring the simulator runs.
//
// Blocks are a blank line apart, each a ```dot fence or a Markdown table,
// and book/book.shale holds the book's source to each, byte for byte. Every
// table is the simulator's answer on every input it lists.
// A unity build, as main.c is.

#include "vm.c"
#include "circuit.c"

#include <string.h>

static Net NET;
static int VALUES[CIRCUIT_MAX_WIRES];

static const char *const NEXT_NAMES[4] = {"next_0", "next_1", "next_2", "next_3"};

static int port(const char *part, const char *name) { return VALUES[net_port_wire(&NET, part, name)]; }

// A part's drawing: its inputs on the left, its outputs on the right.
static const DotStyle PART = {"LR", 0, 1};

static void dot_block(const char *part, DotStyle style) {
    printf("```dot\n");
    net_dot(&NET, part, style, stdout);
    printf("```\n\n");
}

static OneHot four_inputs(const char *const names[4]) {
    net_init(&NET);
    OneHot v;
    for (int k = 0; k < 4; k++) v.w[k] = net_input(&NET, names[k]);
    return v;
}

static void decoder(void) {
    net_init(&NET);
    int hb = net_input(&NET, "hb"), lb = net_input(&NET, "lb");
    part_decoder(&NET, "decoder", hb, lb, IS);
    dot_block("decoder", PART);
    printf("| AX | hb | lb | is_0 | is_1 | is_2 | is_3 |\n|---|---|---|---|---|---|---|\n");
    for (int x = 0; x < 4; x++) {
        int in[2] = {x / 2, x % 2};
        net_eval(&NET, in, VALUES);
        printf("| %d | %d | %d |", x, x / 2, x % 2);
        for (int k = 0; k < 4; k++) printf(" %d |", port("decoder", IS[k]));
        printf("\n");
    }
    printf("\n");
}

static void encoder(void) {
    OneHot v = four_inputs(NEXT_NAMES);
    int bits[2];
    part_encoder(&NET, "encoder", v, bits);
    dot_block("encoder", PART);
    printf("| lit wire | hb' | lb' |\n|---|---|---|\n");
    for (int x = 0; x < 4; x++) {
        int in[4] = {x == 0, x == 1, x == 2, x == 3};
        net_eval(&NET, in, VALUES);
        printf("| next_%d | %d | %d |\n", x, port("encoder", "hb'"), port("encoder", "lb'"));
    }
    printf("\n");
}

typedef MachineOut (*Build)(Net *, const char *, OneHot);
static const Build BUILD[4] = {part_nada, part_zero, part_decr, part_mod2};

static void machines(void) {
    for (int op = 0; op < NUM_OPS; op++) {
        OneHot is = four_inputs(IS);
        BUILD[op](&NET, op_name((Op)op), is);
        dot_block(op_name((Op)op), PART);
    }
    printf("| AX | `nada` | `zero` | `decr` | `mod2` |\n|---|---|---|---|---|\n");
    for (int x = 0; x < 4; x++) {
        printf("| %d |", x);
        for (int op = 0; op < NUM_OPS; op++) {
            OneHot is = four_inputs(IS);
            const char *part = op_name((Op)op);
            BUILD[op](&NET, part, is);
            int in[4] = {x == 0, x == 1, x == 2, x == 3};
            net_eval(&NET, in, VALUES);
            const char *sep = " ";
            for (int k = 0; k < 4; k++) {
                if (port(part, NEXT_NAMES[k])) {
                    printf("%snext_%d", sep, k);
                    sep = ", ";
                }
            }
            if (port(part, "accepts")) {
                printf("%saccepts", sep);
                sep = ", ";
            }
            if (port(part, "halts")) printf("%shalts", sep);
            printf(" |");
        }
        printf("\n");
    }
    printf("\n");
}

static void hold(void) {
    OneHot op = four_inputs(OPS);
    int h = net_input(&NET, "halted");
    part_hold(&NET, "hold", op, h);
    dot_block("hold", PART);
    printf("| halted | op | en_nada | en_zero | en_decr | en_mod2 |\n|---|---|---|---|---|---|\n");
    for (int halted = 0; halted < 2; halted++) {
        for (int x = 0; x < 4; x++) {
            int in[5] = {x == 0, x == 1, x == 2, x == 3, halted};
            net_eval(&NET, in, VALUES);
            printf("| %d | `%s` |", halted, op_name((Op)x));
            for (int k = 0; k < 4; k++) printf(" %d |", port("hold", EN[k]));
            printf("\n");
        }
    }
    printf("\n");
}

static void selector(void) {
    static const char *const D[4] = {"nada's", "zero's", "decr's", "mod2's"};
    net_init(&NET);
    OneHot en;
    int driver[4];
    for (int k = 0; k < 4; k++) en.w[k] = net_input(&NET, EN[k]);
    for (int k = 0; k < 4; k++) driver[k] = net_input(&NET, D[k]);
    part_select_wire(&NET, "selector", "next_1", en, driver);
    dot_block("selector", PART);
}

static void latches(void) {
    net_init(&NET);
    int halted = net_input(&NET, "halted"), halts = net_input(&NET, "halts");
    int accepted = net_input(&NET, "accepted"), accepts = net_input(&NET, "accepts");
    part_latch(&NET, "latches", "halted'", halted, halts);
    part_latch(&NET, "latches", "accepted'", accepted, accepts);
    dot_block("latches", PART);
    printf("| accepted | accepts | accepted' |\n|---|---|---|\n");
    for (int v = 0; v < 4; v++) {
        int in[4] = {0, 0, v >> 1, v & 1};
        net_eval(&NET, in, VALUES);
        printf("| %d | %d | %d |\n", v >> 1, v & 1, port("latches", "accepted'"));
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
    static const char *const PARTS[] = {"decoder", "op decoder", "hold", "nada", "zero", "decr", "mod2", "selector",
                                        "encoder", "latches"};
    circuit_stepper(&NET);
    printf("| part | NOT | AND | OR | gates |\n|---|---|---|---|---|\n");
    for (size_t i = 0; i < sizeof PARTS / sizeof PARTS[0]; i++) count_row(PARTS[i], net_count(&NET, PARTS[i]));
    count_row("**the stepper**", net_count(&NET, ""));
    circuit_core(&NET);
    count_row("**a core**, six steppers", net_count(&NET, ""));
    circuit_decides(&NET);
    count_row("**the decider**, four cores and the comparators", net_count(&NET, ""));
    printf("\n");
}

static void boxes_block(const char *scope, const char *rankdir) {
    printf("```dot\n");
    net_boxes_dot(&NET, scope, rankdir, stdout);
    printf("```\n\n");
}

// Each part of a stepper as a box: how many wires go in and come out, and
// how many gates are inside, read from the stepper's links and ports.
static void vocabulary(void) {
    static const char *const PARTS[] = {"decoder", "op decoder", "hold", "nada", "zero", "decr", "mod2", "selector",
                                        "encoder", "latches"};
    circuit_stepper(&NET);
    printf("| box | wires in | wires out | gates |\n|---|---|---|---|\n");
    for (size_t i = 0; i < sizeof PARTS / sizeof PARTS[0]; i++) {
        int in = 0, out = 0;
        for (int k = 0; k < NET.num_links; k++) {
            if (strcmp(NET.links[k].scope, "") == 0 && strcmp(NET.links[k].to, PARTS[i]) == 0) in++;
        }
        for (int k = 0; k < NET.num_ports; k++) {
            if (strcmp(NET.ports[k].part, PARTS[i]) == 0) out++;
        }
        GateCount c = net_count(&NET, PARTS[i]);
        printf("| %s | %d | %d | %d |\n", PARTS[i], in, out, c.nots + c.ands + c.ors);
    }
    printf("\n");
}

int main(int argc, char **argv) {
    (void)argv;
    if (argc != 1) {
        fprintf(stderr, "usage: circuit_main\n");
        return 2;
    }
    decoder();
    encoder();
    machines();
    hold();
    selector();
    latches();
    vocabulary();
    circuit_stepper(&NET);
    boxes_block("", "TB");
    counts();
    circuit_core(&NET);
    boxes_block("", "TB");
    circuit_decides(&NET);
    boxes_block("", "LR");
    return fflush(stdout) == 0 ? 0 : 1;
}
