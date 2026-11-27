// circuit.c: the machine as a netlist of AND, OR and NOT gates (circuit.h).

#include "circuit.h"

#include <stdlib.h>
#include <string.h>

static void circuit_die(const char *what, long value) {
    fprintf(stderr, "circuit: %s: %ld\n", what, value);
    abort();
}

static void copy_name(char *to, const char *from) {
    size_t n = strlen(from);
    if (n >= CIRCUIT_NAME) circuit_die("a name past the limit", (long)n);
    memcpy(to, from, n + 1);
}

static int new_wire(Net *net, WireKind kind, const char *part, const char *name, int a, int b) {
    if (net->num_wires == CIRCUIT_MAX_WIRES) circuit_die("too many wires", net->num_wires);
    if (a >= net->num_wires || b >= net->num_wires) circuit_die("a gate reads a wire not yet made", a > b ? a : b);
    Wire *w = &net->wires[net->num_wires];
    w->kind = kind;
    w->a = a;
    w->b = b;
    copy_name(w->name, name);
    copy_name(w->part, part);
    return net->num_wires++;
}

void net_init(Net *net) {
    net->num_wires = 0;
    net->num_ports = 0;
    net->num_links = 0;
    new_wire(net, W_ZERO, "", "0", -1, -1);
    new_wire(net, W_ONE, "", "1", -1, -1);
}

int net_input(Net *net, const char *name) { return new_wire(net, W_INPUT, "", name, -1, -1); }

int net_alias(Net *net, const char *part, const char *name, int a) { return new_wire(net, W_ALIAS, part, name, a, -1); }

int net_not(Net *net, const char *part, const char *name, int a) { return new_wire(net, W_NOT, part, name, a, -1); }

int net_and(Net *net, const char *part, const char *name, int a, int b) {
    return new_wire(net, W_AND, part, name, a, b);
}

int net_or(Net *net, const char *part, const char *name, int a, int b) {
    return new_wire(net, W_OR, part, name, a, b);
}

void net_port(Net *net, const char *part, const char *name, int wire) {
    if (net->num_ports == CIRCUIT_MAX_PORTS) circuit_die("too many ports", net->num_ports);
    Port *p = &net->ports[net->num_ports++];
    copy_name(p->part, part);
    copy_name(p->name, name);
    p->wire = wire;
}

void net_link(Net *net, const char *scope, const char *to, const char *to_name, const char *from, const char *from_name) {
    if (net->num_links == CIRCUIT_MAX_LINKS) circuit_die("too many links", net->num_links);
    Link *l = &net->links[net->num_links++];
    copy_name(l->scope, scope);
    copy_name(l->to, to);
    copy_name(l->to_name, to_name);
    copy_name(l->from, from);
    copy_name(l->from_name, from_name);
}

int net_port_wire(const Net *net, const char *part, const char *name) {
    for (int i = 0; i < net->num_ports; i++) {
        if (strcmp(net->ports[i].part, part) == 0 && strcmp(net->ports[i].name, name) == 0) return net->ports[i].wire;
    }
    fprintf(stderr, "circuit: no port %s of part \"%s\"\n", name, part);
    abort();
    return -1; // not reached; tcc can't see that abort doesn't return
}

static int is_gate(WireKind k) { return k == W_NOT || k == W_AND || k == W_OR; }

// Whether `part` is `scope` or a part inside it.
static int in_scope(const char *part, const char *scope) {
    size_t n = strlen(scope);
    if (n == 0) return 1;
    return strncmp(part, scope, n) == 0 && (part[n] == '\0' || part[n] == ' ');
}

GateCount net_count(const Net *net, const char *part) {
    GateCount c = {0, 0, 0, 0};
    for (int i = 0; i < net->num_wires; i++) {
        const Wire *w = &net->wires[i];
        if (!is_gate(w->kind) || !in_scope(w->part, part)) continue;
        if (w->kind == W_NOT) c.nots++;
        if (w->kind == W_AND) c.ands++;
        if (w->kind == W_OR) c.ors++;
    }
    return c;
}

void net_eval(const Net *net, const int *inputs, int *values) {
    int next_input = 0;
    for (int i = 0; i < net->num_wires; i++) {
        const Wire *w = &net->wires[i];
        switch (w->kind) {
        case W_ZERO: values[i] = 0; break;
        case W_ONE: values[i] = 1; break;
        case W_INPUT: values[i] = inputs[next_input++] ? 1 : 0; break;
        case W_NOT: values[i] = !values[w->a]; break;
        case W_AND: values[i] = values[w->a] & values[w->b]; break;
        case W_OR: values[i] = values[w->a] | values[w->b]; break;
        case W_ALIAS: values[i] = values[w->a]; break;
        }
    }
}

// ── The parts ───────────────────────────────────────────────────────────

static const char *const MACHINES[4] = {"nada", "zero", "decr", "mod2"};

static void port_machine(Net *net, const char *part, MachineOut m) {
    net_port(net, part, "hb'", m.hb);
    net_port(net, part, "lb'", m.lb);
    if (m.accepts != WIRE_0) net_port(net, part, "accepts", m.accepts);
    if (m.halts != WIRE_0) net_port(net, part, "halts", m.halts);
}

// nada: both bits pass.
MachineOut part_nada(Net *net, const char *part, int hb, int lb) {
    MachineOut m = {hb, lb, WIRE_0, WIRE_0};
    port_machine(net, part, m);
    return m;
}

// zero: both bits pass, and NOR(hb, lb), AX = 0, accepts. The run goes on
// after an accept, as every step of a circuit runs: the accepted latch holds
// the verdict, and halted stays off, as Steve's stepper has it.
MachineOut part_zero(Net *net, const char *part, int hb, int lb) {
    int either = net_or(net, part, "hb_or_lb", hb, lb);
    MachineOut m = {hb, lb, net_not(net, part, "accepts", either), WIRE_0};
    port_machine(net, part, m);
    return m;
}

// decr, the two-bit decrementer: the high bit survives only when the low
// bit pays (3 to 2), the low bit always flips, and NOR(hb, lb), AX = 0,
// halts. At AX = 0 the bits come out 0 1.
MachineOut part_decr(Net *net, const char *part, int hb, int lb) {
    int high = net_and(net, part, "hb'", hb, lb);
    int low = net_not(net, part, "lb'", lb);
    int either = net_or(net, part, "hb_or_lb", hb, lb);
    MachineOut m = {high, low, WIRE_0, net_not(net, part, "halts", either)};
    port_machine(net, part, m);
    return m;
}

// mod2: taking 2 when AX is 2 or 3 clears the high bit; lb passes.
MachineOut part_mod2(Net *net, const char *part, int hb, int lb) {
    (void)hb;
    MachineOut m = {WIRE_0, lb, WIRE_0, WIRE_0};
    port_machine(net, part, m);
    return m;
}

int part_mux2(Net *net, const char *part, int s, int in0, int in1) {
    int not_s = net_not(net, part, "not_s", s);
    int pick0 = net_and(net, part, "pick0", in0, not_s);
    int pick1 = net_and(net, part, "pick1", in1, s);
    int out = net_or(net, part, "out", pick0, pick1);
    net_port(net, part, "out", out);
    return out;
}

static void part_name(char *to, const char *prefix, const char *name);

int part_mux4(Net *net, const char *part, int s_hb, int s_lb, const int in[4]) {
    char p[CIRCUIT_NAME];
    part_name(p, part, " low");
    int low = part_mux2(net, p, s_lb, in[0], in[1]);
    part_name(p, part, " high");
    int high = part_mux2(net, p, s_lb, in[2], in[3]);
    part_name(p, part, " top");
    int out = part_mux2(net, p, s_hb, low, high);
    net_port(net, part, "out", out);
    net_link(net, part, "low", "s", "", "s_lb");
    net_link(net, part, "low", "in 0", "", "in 0");
    net_link(net, part, "low", "in 1", "", "in 1");
    net_link(net, part, "high", "s", "", "s_lb");
    net_link(net, part, "high", "in 0", "", "in 2");
    net_link(net, part, "high", "in 1", "", "in 3");
    net_link(net, part, "top", "s", "", "s_hb");
    net_link(net, part, "top", "in 0", "low", "out");
    net_link(net, part, "top", "in 1", "high", "out");
    net_link(net, part, "", "out", "top", "out");
    return out;
}

void part_hold(Net *net, const char *part, int op_hb, int op_lb, int halted, int sel[2]) {
    int running = net_not(net, part, "running", halted);
    sel[0] = net_and(net, part, "sel_hb", op_hb, running);
    sel[1] = net_and(net, part, "sel_lb", op_lb, running);
    net_port(net, part, "sel_hb", sel[0]);
    net_port(net, part, "sel_lb", sel[1]);
}

int part_latch(Net *net, const char *part, const char *name, int flag, int news) {
    int out = net_or(net, part, name, flag, news);
    net_port(net, part, name, out);
    return out;
}

static void part_name(char *to, const char *prefix, const char *name) {
    char buf[2 * CIRCUIT_NAME];
    snprintf(buf, sizeof buf, "%s%s", prefix, name);
    copy_name(to, buf);
}

static const char *const MUXES[4] = {"hb mux", "lb mux", "accepts mux", "halts mux"};
static const char *const OUTS[4] = {"hb'", "lb'", "accepts", "halts"};

// How a stepper's boxes are wired, as part_stepper wires them.
static void stepper_links(Net *net, const char *own, const MachineOut m[4]) {
    for (int i = 0; i < 4; i++) {
        net_link(net, own, MACHINES[i], "hb", "", "AX");
        net_link(net, own, MACHINES[i], "lb", "", "AX");
    }
    net_link(net, own, "hold", "op_hb", "", "line");
    net_link(net, own, "hold", "op_lb", "", "line");
    net_link(net, own, "hold", "halted", "", "halted");
    for (int k = 0; k < 4; k++) {
        net_link(net, own, MUXES[k], "s_hb", "hold", "sel_hb");
        net_link(net, own, MUXES[k], "s_lb", "hold", "sel_lb");
        for (int i = 0; i < 4; i++) {
            int w = k == 0 ? m[i].hb : k == 1 ? m[i].lb : k == 2 ? m[i].accepts : m[i].halts;
            // A machine's bit tied to 0 is its own port (mod2's hb'); a flag
            // the machine lacks is the mux's input tied to 0.
            if (k < 2 || w != WIRE_0) net_link(net, own, MUXES[k], "in", MACHINES[i], OUTS[k]);
            else net_link(net, own, MUXES[k], "in", "", "0");
        }
    }
    net_link(net, own, "latches", "halted", "", "halted");
    net_link(net, own, "latches", "halts", "halts mux", "out");
    net_link(net, own, "latches", "accepted", "", "accepted");
    net_link(net, own, "latches", "accepts", "accepts mux", "out");
    net_link(net, own, "", "hb'", "hb mux", "out");
    net_link(net, own, "", "lb'", "lb mux", "out");
    net_link(net, own, "", "halted'", "latches", "halted'");
    net_link(net, own, "", "accepted'", "latches", "accepted'");
}

CircuitState part_stepper(Net *net, const char *prefix, int k, CircuitState s, int op_hb, int op_lb) {
    char step[CIRCUIT_NAME], p[CIRCUIT_NAME];
    if (k > 0) {
        char buf[CIRCUIT_NAME];
        snprintf(buf, sizeof buf, "step %d ", k);
        part_name(step, prefix, buf);
    } else {
        part_name(step, prefix, "");
    }
    // The stepper's own name, without the trailing space: the scope of its links.
    char own[CIRCUIT_NAME];
    copy_name(own, step);
    size_t n = strlen(own);
    if (n > 0) own[n - 1] = '\0';
    MachineOut m[4];
    part_name(p, step, "nada");
    m[0] = part_nada(net, p, s.hb, s.lb);
    part_name(p, step, "zero");
    m[1] = part_zero(net, p, s.hb, s.lb);
    part_name(p, step, "decr");
    m[2] = part_decr(net, p, s.hb, s.lb);
    part_name(p, step, "mod2");
    m[3] = part_mod2(net, p, s.hb, s.lb);
    part_name(p, step, "hold");
    int sel[2];
    part_hold(net, p, op_hb, op_lb, s.halted, sel);
    int out[4];
    for (int j = 0; j < 4; j++) {
        int in[4];
        for (int i = 0; i < 4; i++) in[i] = j == 0 ? m[i].hb : j == 1 ? m[i].lb : j == 2 ? m[i].accepts : m[i].halts;
        part_name(p, step, MUXES[j]);
        out[j] = part_mux4(net, p, sel[0], sel[1], in);
    }
    part_name(p, step, "latches");
    CircuitState next;
    next.hb = out[0];
    next.lb = out[1];
    next.halted = part_latch(net, p, "halted'", s.halted, out[3]);
    next.accepted = part_latch(net, p, "accepted'", s.accepted, out[2]);
    stepper_links(net, own, m);
    net_port(net, own, "hb'", next.hb);
    net_port(net, own, "lb'", next.lb);
    net_port(net, own, "halted'", next.halted);
    net_port(net, own, "accepted'", next.accepted);
    return next;
}

// How a core's six stepper boxes are wired: the first from the input and
// the flags' constant 0, each later one from the one before it, and each
// from its line's two bits.
static void core_links(Net *net, const char *own) {
    static const char *const STATE[4] = {"hb", "lb", "halted", "accepted"};
    static const char *const LEFT[4] = {"hb'", "lb'", "halted'", "accepted'"};
    char step[CIRCUIT_NAME], before[CIRCUIT_NAME], line[CIRCUIT_NAME];
    for (int k = 1; k <= 6; k++) {
        snprintf(step, sizeof step, "step %d", k);
        snprintf(before, sizeof before, "step %d", k - 1);
        snprintf(line, sizeof line, "line %d", k);
        for (int i = 0; i < 4; i++) {
            if (k > 1) net_link(net, own, step, STATE[i], before, LEFT[i]);
            else net_link(net, own, step, STATE[i], "", i < 2 ? "AX" : "0");
        }
        net_link(net, own, step, "op_hb", "", line);
        net_link(net, own, step, "op_lb", "", line);
    }
    net_link(net, own, "", "accepted", "step 6", "accepted'");
}

CircuitState part_core(Net *net, const char *prefix, CircuitState start, const int prog[12]) {
    CircuitState s = start;
    for (int k = 1; k <= 6; k++) s = part_stepper(net, prefix, k, s, prog[2 * (k - 1)], prog[2 * (k - 1) + 1]);
    char own[CIRCUIT_NAME];
    copy_name(own, prefix);
    size_t n = strlen(own);
    if (n > 0) own[n - 1] = '\0';
    core_links(net, own);
    return s;
}

void circuit_stepper(Net *net) {
    net_init(net);
    CircuitState s;
    s.hb = net_input(net, "hb");
    s.lb = net_input(net, "lb");
    s.halted = net_input(net, "halted");
    s.accepted = net_input(net, "accepted");
    int op_hb = net_input(net, "op_hb");
    int op_lb = net_input(net, "op_lb");
    part_stepper(net, "", 0, s, op_hb, op_lb);
}

static void program_inputs(Net *net, int prog[12]) {
    char name[CIRCUIT_NAME];
    for (int i = 0; i < 6; i++) {
        snprintf(name, sizeof name, "op%d_hb", i + 1);
        prog[2 * i] = net_input(net, name);
        snprintf(name, sizeof name, "op%d_lb", i + 1);
        prog[2 * i + 1] = net_input(net, name);
    }
}

void circuit_core(Net *net) {
    net_init(net);
    int prog[12];
    program_inputs(net, prog);
    CircuitState start;
    start.hb = net_input(net, "in_hb");
    start.lb = net_input(net, "in_lb");
    start.halted = WIRE_0;
    start.accepted = WIRE_0;
    CircuitState end = part_core(net, "", start, prog);
    net_port(net, "", "accepted", end.accepted);
}

// How the decider's boxes for input n are wired: core n from the program
// and its input's constants, its comparator from it and the want bit, and
// the AND from the comparator.
static void decider_links(Net *net, int n) {
    char core[CIRCUIT_NAME], same[CIRCUIT_NAME], ax[CIRCUIT_NAME], want[CIRCUIT_NAME];
    snprintf(core, sizeof core, "core %d", n);
    snprintf(same, sizeof same, "same %d", n);
    snprintf(ax, sizeof ax, "AX = %d", n);
    snprintf(want, sizeof want, "want_%d", n);
    for (int i = 0; i < 12; i++) net_link(net, "", core, "program", "", "program");
    net_link(net, "", core, "in_hb", "", ax);
    net_link(net, "", core, "in_lb", "", ax);
    net_link(net, "", same, "accepted", core, "accepted");
    net_link(net, "", same, "want", "", want);
    net_link(net, "", "all", same, same, "same?");
}

void circuit_decides(Net *net) {
    net_init(net);
    int prog[12];
    program_inputs(net, prog);
    int want[4];
    char name[CIRCUIT_NAME];
    for (int n = 0; n < 4; n++) {
        snprintf(name, sizeof name, "want%d", n);
        want[n] = net_input(net, name);
    }
    int all = -1;
    for (int n = 0; n < 4; n++) {
        char prefix[CIRCUIT_NAME];
        snprintf(prefix, sizeof prefix, "core %d ", n);
        // The core's input, AX = n, as named copies of the constants.
        char core[CIRCUIT_NAME];
        snprintf(core, sizeof core, "core %d", n);
        int in_hb = net_alias(net, core, "in_hb", n / 2 ? WIRE_1 : WIRE_0);
        int in_lb = net_alias(net, core, "in_lb", n % 2 ? WIRE_1 : WIRE_0);
        CircuitState start = {in_hb, in_lb, WIRE_0, WIRE_0};
        CircuitState end = part_core(net, prefix, start, prog);
        // same? is (a AND w) OR (NOT a AND NOT w).
        char part[CIRCUIT_NAME];
        snprintf(part, sizeof part, "same %d", n);
        int a = end.accepted;
        int both = net_and(net, part, "both1", a, want[n]);
        int not_a = net_not(net, part, "not_accepted", a);
        int not_w = net_not(net, part, "not_want", want[n]);
        int neither = net_and(net, part, "both0", not_a, not_w);
        int same = net_or(net, part, "same", both, neither);
        char so_far[CIRCUIT_NAME];
        snprintf(so_far, sizeof so_far, "upto%d", n);
        all = all < 0 ? same : net_and(net, "all", so_far, all, same);
        decider_links(net, n);
    }
    net_link(net, "", "", "decides", "all", "decides");
    net_port(net, "", "decides", all);
}

// ── Equations ───────────────────────────────────────────────────────────

// A part's name as an equation's prefix: words joined with dots, a number
// joined to the word before it, and `mux` joined to the word before it with
// an underscore.
static void equation_part(const char *part, char *out, size_t size) {
    size_t n = 0;
    out[0] = '\0';
    const char *s = part;
    while (*s) {
        while (*s == ' ') s++;
        if (*s == '\0') break;
        const char *end = s;
        while (*end && *end != ' ') end++;
        size_t len = (size_t)(end - s);
        int digits = 1;
        for (const char *p = s; p < end; p++) {
            if (*p < '0' || *p > '9') digits = 0;
        }
        const char *glue = n == 0 ? "" : digits ? "" : (len == 3 && strncmp(s, "mux", 3) == 0) ? "_" : ".";
        if (n + strlen(glue) + len + 1 > size) circuit_die("an equation's name past the limit", (long)(n + len));
        memcpy(out + n, glue, strlen(glue));
        n += strlen(glue);
        memcpy(out + n, s, len);
        n += len;
        out[n] = '\0';
        s = end;
    }
}

static int part_words(const char *part) {
    int words = 0, in_word = 0;
    for (; *part; part++) {
        if (*part != ' ' && !in_word) words++;
        in_word = *part != ' ';
    }
    return words;
}

static void equation_name(const Net *net, int i, char *out, size_t size) {
    const Wire *w = &net->wires[i];
    char prefix[2 * CIRCUIT_NAME];
    const char *part = w->part;
    const char *name = w->name;
    if (is_gate(w->kind)) {
        int best = -1;
        for (int p = 0; p < net->num_ports; p++) {
            if (net->ports[p].wire != i) continue;
            if (best < 0 || part_words(net->ports[p].part) < part_words(net->ports[best].part)) best = p;
        }
        if (best >= 0) {
            part = net->ports[best].part;
            name = net->ports[best].name;
        }
    }
    equation_part(part, prefix, sizeof prefix);
    if (prefix[0] == '\0') snprintf(out, size, "%s", name);
    else snprintf(out, size, "%s.%s", prefix, name);
}

void net_equations(const Net *net, FILE *f) {
    enum { NAME = 3 * CIRCUIT_NAME };
    static char names[CIRCUIT_MAX_WIRES][NAME];
    for (int i = 0; i < net->num_wires; i++) equation_name(net, i, names[i], NAME);
    for (int i = 2; i < net->num_wires; i++) {
        for (int j = i + 1; j < net->num_wires; j++) {
            if (strcmp(names[i], names[j]) == 0) {
                fprintf(stderr, "circuit: the equation name %s is made twice\n", names[i]);
                abort();
            }
        }
    }
    fprintf(f, "inputs:");
    for (int i = 0; i < net->num_wires; i++) {
        if (net->wires[i].kind == W_INPUT) fprintf(f, " %s", names[i]);
    }
    fprintf(f, "\n");
    for (int i = 0; i < net->num_wires; i++) {
        const Wire *w = &net->wires[i];
        switch (w->kind) {
        case W_ALIAS: fprintf(f, "%s = %s\n", names[i], names[w->a]); break;
        case W_NOT: fprintf(f, "%s = NOT %s\n", names[i], names[w->a]); break;
        case W_AND: fprintf(f, "%s = %s AND %s\n", names[i], names[w->a], names[w->b]); break;
        case W_OR: fprintf(f, "%s = %s OR %s\n", names[i], names[w->a], names[w->b]); break;
        default: break;
        }
    }
}

// ── Drawing ─────────────────────────────────────────────────────────────

// A name as Graphviz or SVG text: a quote, a backslash, `<`, `>` and `&`
// escaped for the one that reads it.
static void dot_text(FILE *f, const char *s) {
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') fputc('\\', f);
        fputc(*s, f);
    }
}

static void svg_text(FILE *f, const char *s) {
    for (; *s; s++) {
        if (*s == '<') fputs("&lt;", f);
        else if (*s == '>') fputs("&gt;", f);
        else if (*s == '&') fputs("&amp;", f);
        else if (*s == '\'') fputs("&#8242;", f); // a prime, as hb' is read
        else fputc(*s, f);
    }
}

// The schematic's grid, in pixels.
enum { SVG_LEFT = 70, SVG_PITCH = 110, SVG_ROW = 64, SVG_TOP = 30, SVG_GATE_W = 56 };

typedef struct {
    int drawn;   // drawn as a symbol of its own
    int fused;   // an OR drawn inside the NOR of the NOT that reads it
    int nor;     // a NOT drawn as a NOR, its inputs the fused OR's
    int column;  // 0 for an input, else its depth
    int row;
    int x, y;    // the output pin
} SvgNode;

static SvgNode SVG[CIRCUIT_MAX_WIRES];

// The wires a drawn gate reads: one for a NOT, the OR's two for a NOR.
static int svg_inputs(const Net *net, int i, int in[2]) {
    const Wire *w = &net->wires[i];
    if (SVG[i].nor) {
        in[0] = net->wires[w->a].a;
        in[1] = net->wires[w->a].b;
        return 2;
    }
    in[0] = w->a;
    in[1] = w->b;
    return w->kind == W_NOT ? 1 : 2;
}

// The pin of input `k` of `n` of a gate whose top is (x, top).
static int svg_pin_y(int top, int k, int n) { return n == 1 ? top + 20 : top + (k == 0 ? 10 : 30); }

static void svg_gate(const Net *net, int i, int x, int top, FILE *f) {
    WireKind k = net->wires[i].kind;
    if (SVG[i].nor || k == W_OR) {
        fprintf(f, "  <path d=\"M%d,%d Q%d,%d %d,%d Q%d,%d %d,%d Q%d,%d %d,%d Z\"/>\n", x, top, x + 12, top + 20, x,
                top + 40, x + 28, top + 40, x + 44, top + 20, x + 28, top, x, top);
        if (SVG[i].nor) fprintf(f, "  <circle cx=\"%d\" cy=\"%d\" r=\"5\"/>\n", x + 49, top + 20);
    } else if (k == W_AND) {
        fprintf(f, "  <path d=\"M%d,%d H%d A20,20 0 0 1 %d,%d H%d Z\"/>\n", x, top, x + 24, x + 24, top + 40, x);
    } else {
        fprintf(f, "  <path d=\"M%d,%d L%d,%d L%d,%d Z\"/>\n", x, top + 6, x + 30, top + 20, x, top + 34);
        fprintf(f, "  <circle cx=\"%d\" cy=\"%d\" r=\"5\"/>\n", x + 35, top + 20);
    }
}

static const char *svg_kind(const Net *net, int i) {
    if (SVG[i].nor) return "NOR";
    if (net->wires[i].kind == W_AND) return "AND";
    if (net->wires[i].kind == W_OR) return "OR";
    return "";
}

// One wire from (sx, sy) to (tx, ty), bending at x = bend.
static void svg_wire(int sx, int sy, int tx, int ty, int bend, FILE *f) {
    if (sy == ty) fprintf(f, "  <polyline points=\"%d,%d %d,%d\"/>\n", sx, sy, tx, ty);
    else fprintf(f, "  <polyline points=\"%d,%d %d,%d %d,%d %d,%d\"/>\n", sx, sy, bend, sy, bend, ty, tx, ty);
}

typedef struct {
    int source;
    int tx, ty;
    int bend;
} SvgWire;

// Places items at their wanted heights, in order of them, each at least
// `gap` below the one above it.
static void svg_spread(int *ys, const int *wanted, int n, int first, int gap) {
    int order[64];
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 1; i < n; i++) {
        for (int j = i; j > 0 && wanted[order[j]] < wanted[order[j - 1]]; j--) {
            int t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    }
    int at = first;
    for (int i = 0; i < n; i++) {
        int y = wanted[order[i]] > at ? wanted[order[i]] : at;
        ys[order[i]] = y;
        at = y + gap;
    }
}

void net_svg(const Net *net, const char *part, FILE *f) {
    static SvgWire wires[256];
    int num_wires = 0;
    memset(SVG, 0, sizeof SVG);
    for (int i = 0; i < net->num_wires; i++) {
        const Wire *w = &net->wires[i];
        SVG[i].drawn = is_gate(w->kind) && in_scope(w->part, part);
    }
    // An OR read by one NOT alone, and by no port, is drawn inside a NOR.
    for (int i = 0; i < net->num_wires; i++) {
        if (!SVG[i].drawn || net->wires[i].kind != W_OR) continue;
        int readers = 0, reader = -1;
        for (int j = i + 1; j < net->num_wires; j++) {
            const Wire *w = &net->wires[j];
            if (!is_gate(w->kind)) continue;
            if (w->a == i || (w->kind != W_NOT && w->b == i)) {
                readers++;
                reader = j;
            }
        }
        for (int p = 0; p < net->num_ports; p++) {
            if (net->ports[p].wire == i) readers++;
        }
        if (readers == 1 && reader >= 0 && net->wires[reader].kind == W_NOT && SVG[reader].drawn) {
            SVG[i].drawn = 0;
            SVG[i].fused = 1;
            SVG[reader].nor = 1;
        }
    }
    // The inputs down the left, a row apart.
    int inputs = 0;
    for (int i = 0; i < net->num_wires; i++) {
        if (net->wires[i].kind != W_INPUT) continue;
        SVG[i].x = SVG_LEFT;
        SVG[i].y = SVG_TOP + inputs * SVG_ROW;
        inputs++;
    }
    int bottom = SVG_TOP + (inputs - 1) * SVG_ROW;
    // Columns: each gate one past the deepest wire it reads, at the mean
    // height of what it reads, in the order the gates were made, each at
    // least a gate's height and a margin below the one above it.
    int columns = 0;
    for (int i = 0; i < net->num_wires; i++) {
        if (!SVG[i].drawn) continue;
        int in[2];
        int n = svg_inputs(net, i, in);
        int depth = 0;
        for (int k = 0; k < n; k++) {
            if (SVG[in[k]].column > depth) depth = SVG[in[k]].column;
        }
        SVG[i].column = depth + 1;
        if (SVG[i].column > columns) columns = SVG[i].column;
    }
    for (int c = 1; c <= columns; c++) {
        int ids[64], wanted[64], ys[64];
        int n = 0;
        for (int i = 0; i < net->num_wires; i++) {
            if (!SVG[i].drawn || SVG[i].column != c) continue;
            int in[2];
            int k = svg_inputs(net, i, in);
            int sum = 0;
            for (int j = 0; j < k; j++) sum += SVG[in[j]].y;
            ids[n] = i;
            wanted[n] = sum / k;
            n++;
        }
        int at = SVG_TOP;
        for (int j = 0; j < n; j++) {
            ys[j] = wanted[j] > at ? wanted[j] : at;
            at = ys[j] + 56;
        }
        for (int j = 0; j < n; j++) {
            int i = ids[j];
            WireKind k = net->wires[i].kind;
            int right = SVG[i].nor ? 54 : k == W_NOT ? 40 : 44;
            SVG[i].y = ys[j];
            SVG[i].x = SVG_LEFT + c * SVG_PITCH - SVG_GATE_W + right;
            if (ys[j] > bottom) bottom = ys[j];
        }
    }
    // The outputs down the right, each at its source's height where it can
    // be; a constant output takes the height of the input in its row.
    int out_x = SVG_LEFT + (columns + 1) * SVG_PITCH - SVG_PITCH / 2;
    if (columns == 0) out_x = SVG_LEFT + SVG_PITCH + 20;
    int outs[64], wanted[64], ys[64];
    int outputs = 0;
    for (int p = 0; p < net->num_ports; p++) {
        if (strcmp(net->ports[p].part, part) != 0) continue;
        int source = net->ports[p].wire;
        outs[outputs] = p;
        wanted[outputs] = source == WIRE_0 ? SVG_TOP + outputs * SVG_ROW : SVG[source].y;
        outputs++;
    }
    svg_spread(ys, wanted, outputs, SVG_TOP, 32);
    for (int j = 0; j < outputs; j++) {
        if (ys[j] > bottom) bottom = ys[j];
    }
    int width = out_x + 80;
    int height = bottom + SVG_TOP;
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" class=\"schematic\" width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\" "
               "style=\"max-width:100%%;height:auto\" font-family=\"Helvetica, Arial, sans-serif\" font-size=\"14\">\n",
            width, height, width, height);
    // The wires into each drawn gate, bending in its column's channel, the
    // higher source to the higher pin.
    int channel[64] = {0};
    for (int i = 0; i < net->num_wires; i++) {
        if (!SVG[i].drawn) continue;
        int in[2];
        int n = svg_inputs(net, i, in);
        if (n == 2 && SVG[in[1]].y < SVG[in[0]].y) {
            int t = in[0];
            in[0] = in[1];
            in[1] = t;
        }
        int gx = SVG_LEFT + SVG[i].column * SVG_PITCH - SVG_GATE_W;
        int top = SVG[i].y - 20;
        for (int k = 0; k < n; k++) {
            int indent = net->wires[i].kind == W_OR || SVG[i].nor ? 6 : 0;
            int bend = gx - 14 - 10 * channel[SVG[i].column]++;
            int pin = svg_pin_y(top, k, n);
            // A source within a few pixels of its pin runs straight in.
            if (SVG[in[k]].y - pin <= 6 && pin - SVG[in[k]].y <= 6) pin = SVG[in[k]].y;
            wires[num_wires++] = (SvgWire){in[k], gx + indent, pin, bend};
        }
    }
    fprintf(f, "<g fill=\"currentColor\">\n");
    for (int j = 0; j < outputs; j++) {
        const Port *p = &net->ports[outs[j]];
        if (p->wire == WIRE_0) {
            // A constant 0: a short wire from a 0 beside its output.
            fprintf(f, "<text x=\"%d\" y=\"%d\" text-anchor=\"end\">0</text>\n", out_x - 40, ys[j] + 5);
        } else {
            wires[num_wires++] = (SvgWire){p->wire, out_x, ys[j], out_x - 16 - 10 * j};
        }
        fprintf(f, "<text x=\"%d\" y=\"%d\">", out_x + 8, ys[j] + 5);
        svg_text(f, p->name);
        fprintf(f, "</text>\n");
    }
    for (int i = 0; i < net->num_wires; i++) {
        if (SVG[i].drawn && svg_kind(net, i)[0] != '\0') {
            fprintf(f, "<text x=\"%d\" y=\"%d\" font-size=\"10\" text-anchor=\"middle\">%s</text>\n",
                    SVG_LEFT + SVG[i].column * SVG_PITCH - SVG_GATE_W + 19, SVG[i].y + 4, svg_kind(net, i));
        }
        if (net->wires[i].kind == W_INPUT) {
            fprintf(f, "<text x=\"%d\" y=\"%d\" text-anchor=\"end\">", SVG_LEFT - 8, SVG[i].y + 5);
            svg_text(f, net->wires[i].name);
            fprintf(f, "</text>\n");
        }
    }
    // A dot where a wire branches: at each bend short of the wire's farthest.
    for (int k = 0; k < num_wires; k++) {
        const SvgNode *s = &SVG[wires[k].source];
        if (s->y == wires[k].ty) continue;
        int further = 0;
        for (int j = 0; j < num_wires; j++) {
            if (j == k || wires[j].source != wires[k].source) continue;
            int reach = SVG[wires[j].source].y == wires[j].ty ? wires[j].tx : wires[j].bend;
            if (reach > wires[k].bend) further = 1;
        }
        if (further) fprintf(f, "<circle cx=\"%d\" cy=\"%d\" r=\"3.5\"/>\n", wires[k].bend, s->y);
    }
    fprintf(f, "</g>\n<g stroke=\"currentColor\" stroke-width=\"2\" fill=\"none\">\n");
    for (int k = 0; k < num_wires; k++) {
        const SvgNode *s = &SVG[wires[k].source];
        svg_wire(s->x, s->y, wires[k].tx, wires[k].ty, wires[k].bend, f);
    }
    for (int j = 0; j < outputs; j++) {
        if (net->ports[outs[j]].wire == WIRE_0) fprintf(f, "  <polyline points=\"%d,%d %d,%d\"/>\n", out_x - 34, ys[j], out_x, ys[j]);
    }
    for (int i = 0; i < net->num_wires; i++) {
        if (SVG[i].drawn) svg_gate(net, i, SVG_LEFT + SVG[i].column * SVG_PITCH - SVG_GATE_W, SVG[i].y - 20, f);
    }
    fprintf(f, "</g>\n</svg>\n");
}

// ── Drawing boxes ───────────────────────────────────────────────────────

enum { BOX_MAX_NODES = 64, BOX_MAX_EDGES = 128 };

typedef struct {
    char names[BOX_MAX_NODES][CIRCUIT_NAME];
    int count;
} NameSet;

static int name_index(NameSet *s, const char *name) {
    for (int i = 0; i < s->count; i++) {
        if (strcmp(s->names[i], name) == 0) return i;
    }
    if (s->count == BOX_MAX_NODES) circuit_die("too many ends in one drawing", s->count);
    copy_name(s->names[s->count], name);
    return s->count++;
}

// An end of a line: a box, an input of the scope or an output of it.
typedef struct {
    char kind;
    int index;
} End;

typedef struct {
    End from, to;
    int wires;
} BoxEdge;

void net_boxes_dot(const Net *net, const char *scope, const char *rankdir, FILE *f) {
    static NameSet boxes, inputs, outputs;
    static BoxEdge edges[BOX_MAX_EDGES];
    boxes.count = inputs.count = outputs.count = 0;
    int num_edges = 0;
    for (int i = 0; i < net->num_links; i++) {
        const Link *l = &net->links[i];
        if (strcmp(l->scope, scope) != 0) continue;
        End from, to;
        if (l->from[0] == '\0') from = (End){'i', name_index(&inputs, l->from_name)};
        else from = (End){'b', name_index(&boxes, l->from)};
        if (l->to[0] == '\0') to = (End){'o', name_index(&outputs, l->to_name)};
        else to = (End){'b', name_index(&boxes, l->to)};
        int e = 0;
        while (e < num_edges && !(edges[e].from.kind == from.kind && edges[e].from.index == from.index &&
                                  edges[e].to.kind == to.kind && edges[e].to.index == to.index)) {
            e++;
        }
        if (e == num_edges) {
            if (num_edges == BOX_MAX_EDGES) circuit_die("too many lines in one drawing", num_edges);
            edges[num_edges++] = (BoxEdge){from, to, 0};
        }
        edges[e].wires++;
    }
    // Boxes close together side by side, so a stepper's ten fit the page.
    fprintf(f, "digraph {\n  rankdir=%s;\n  nodesep=0.15;\n  node [shape=box, style=rounded];\n", rankdir);
    for (int i = 0; i < inputs.count; i++) {
        fprintf(f, "  i%d [shape=plaintext, label=\"", i);
        dot_text(f, inputs.names[i]);
        fprintf(f, "\"];\n");
    }
    for (int i = 0; i < boxes.count; i++) {
        fprintf(f, "  b%d [label=\"", i);
        dot_text(f, boxes.names[i]);
        fprintf(f, "\"];\n");
    }
    for (int i = 0; i < outputs.count; i++) {
        fprintf(f, "  o%d [shape=plaintext, label=\"", i);
        dot_text(f, outputs.names[i]);
        fprintf(f, "\"];\n");
    }
    for (int e = 0; e < num_edges; e++) {
        fprintf(f, "  %c%d -> %c%d", edges[e].from.kind, edges[e].from.index, edges[e].to.kind, edges[e].to.index);
        if (edges[e].wires > 1) fprintf(f, " [label=\"%d\"]", edges[e].wires);
        fprintf(f, ";\n");
    }
    fprintf(f, "}\n");
}
