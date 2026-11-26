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
    new_wire(net, W_ZERO, "", "0", -1, -1);
    new_wire(net, W_ONE, "", "1", -1, -1);
}

int net_input(Net *net, const char *name) { return new_wire(net, W_INPUT, "", name, -1, -1); }

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
        }
    }
}

// ── The parts ───────────────────────────────────────────────────────────

static const char *const MACHINES[4] = {"nada", "zero", "decr", "mod2"};
static const char *const NEXT[4] = {"next_0", "next_1", "next_2", "next_3"};

static void port_one_hot(Net *net, const char *part, OneHot v) {
    for (int k = 0; k < 4; k++) net_port(net, part, NEXT[k], v.w[k]);
}

static void port_machine(Net *net, const char *part, MachineOut m) {
    port_one_hot(net, part, m.next);
    net_port(net, part, "accepts", m.accepts);
    net_port(net, part, "halts", m.halts);
}

OneHot part_decoder(Net *net, const char *part, int hb, int lb, const char *const names[4]) {
    char name[2 * CIRCUIT_NAME];
    snprintf(name, sizeof name, "~%s", net->wires[hb].name);
    int not_hb = net_not(net, part, name, hb);
    snprintf(name, sizeof name, "~%s", net->wires[lb].name);
    int not_lb = net_not(net, part, name, lb);
    OneHot v;
    v.w[0] = net_and(net, part, names[0], not_hb, not_lb);
    v.w[1] = net_and(net, part, names[1], not_hb, lb);
    v.w[2] = net_and(net, part, names[2], hb, not_lb);
    v.w[3] = net_and(net, part, names[3], hb, lb);
    for (int k = 0; k < 4; k++) net_port(net, part, names[k], v.w[k]);
    return v;
}

void part_encoder(Net *net, const char *part, OneHot v, int bits[2]) {
    bits[0] = net_or(net, part, "hb'", v.w[2], v.w[3]);
    bits[1] = net_or(net, part, "lb'", v.w[1], v.w[3]);
    net_port(net, part, "hb'", bits[0]);
    net_port(net, part, "lb'", bits[1]);
}

// nada: every wire stays where it is.
MachineOut part_nada(Net *net, const char *part, OneHot is) {
    MachineOut m = {is, WIRE_0, WIRE_0};
    port_machine(net, part, m);
    return m;
}

// zero: every wire stays, and is_0 is tapped to accept and to halt.
MachineOut part_zero(Net *net, const char *part, OneHot is) {
    MachineOut m = {is, is.w[0], is.w[0]};
    port_machine(net, part, m);
    return m;
}

// decr, a shifter: each wire moves down a place, and is_0 falls out as a
// halt.
MachineOut part_decr(Net *net, const char *part, OneHot is) {
    MachineOut m = {{{is.w[1], is.w[2], is.w[3], WIRE_0}}, WIRE_0, is.w[0]};
    port_machine(net, part, m);
    return m;
}

// mod2: 3 folds onto 1 and 2 onto 0.
MachineOut part_mod2(Net *net, const char *part, OneHot is) {
    int fold_0 = net_or(net, part, "fold_0", is.w[0], is.w[2]);
    int fold_1 = net_or(net, part, "fold_1", is.w[1], is.w[3]);
    MachineOut m = {{{fold_0, fold_1, WIRE_0, WIRE_0}}, WIRE_0, WIRE_0};
    port_machine(net, part, m);
    return m;
}

OneHot part_hold(Net *net, const char *part, OneHot op, int halted) {
    static const char *const EN[4] = {"en_nada", "en_zero", "en_decr", "en_mod2"};
    int running = net_not(net, part, "~halted", halted);
    OneHot en;
    en.w[0] = net_or(net, part, EN[0], op.w[0], halted);
    for (int k = 1; k < 4; k++) en.w[k] = net_and(net, part, EN[k], op.w[k], running);
    for (int k = 0; k < 4; k++) net_port(net, part, EN[k], en.w[k]);
    return en;
}

int part_select_wire(Net *net, const char *part, const char *name, OneHot enable, const int driver[4]) {
    int out = -1;
    char gate[CIRCUIT_NAME];
    for (int m = 0; m < 4; m++) {
        if (driver[m] == WIRE_0) continue;
        snprintf(gate, sizeof gate, "%s %s", MACHINES[m], name);
        int picked = net_and(net, part, gate, driver[m], enable.w[m]);
        out = out < 0 ? picked : net_or(net, part, name, out, picked);
    }
    if (out < 0) out = WIRE_0;
    net_port(net, part, name, out);
    return out;
}

MachineOut part_selector(Net *net, const char *part, OneHot enable, const MachineOut m[4]) {
    MachineOut out;
    int driver[4];
    for (int k = 0; k < 4; k++) {
        for (int i = 0; i < 4; i++) driver[i] = m[i].next.w[k];
        out.next.w[k] = part_select_wire(net, part, NEXT[k], enable, driver);
    }
    for (int i = 0; i < 4; i++) driver[i] = m[i].accepts;
    out.accepts = part_select_wire(net, part, "accepts", enable, driver);
    for (int i = 0; i < 4; i++) driver[i] = m[i].halts;
    out.halts = part_select_wire(net, part, "halts", enable, driver);
    return out;
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

CircuitState part_stepper(Net *net, const char *prefix, int k, CircuitState s, int op_hb, int op_lb) {
    static const char *const IS[4] = {"is_0", "is_1", "is_2", "is_3"};
    static const char *const OPS[4] = {"op_nada", "op_zero", "op_decr", "op_mod2"};
    char step[CIRCUIT_NAME], p[CIRCUIT_NAME];
    if (k > 0) {
        char buf[CIRCUIT_NAME];
        snprintf(buf, sizeof buf, "step %d ", k);
        part_name(step, prefix, buf);
    } else {
        part_name(step, prefix, "");
    }
    part_name(p, step, "decoder");
    OneHot is = part_decoder(net, p, s.hb, s.lb, IS);
    part_name(p, step, "op decoder");
    OneHot op = part_decoder(net, p, op_hb, op_lb, OPS);
    part_name(p, step, "hold");
    OneHot en = part_hold(net, p, op, s.halted);
    MachineOut m[4];
    part_name(p, step, "nada");
    m[0] = part_nada(net, p, is);
    part_name(p, step, "zero");
    m[1] = part_zero(net, p, is);
    part_name(p, step, "decr");
    m[2] = part_decr(net, p, is);
    part_name(p, step, "mod2");
    m[3] = part_mod2(net, p, is);
    part_name(p, step, "selector");
    MachineOut picked = part_selector(net, p, en, m);
    part_name(p, step, "encoder");
    int bits[2];
    part_encoder(net, p, picked.next, bits);
    part_name(p, step, "latches");
    CircuitState out;
    out.hb = bits[0];
    out.lb = bits[1];
    out.halted = part_latch(net, p, "halted'", s.halted, picked.halts);
    out.accepted = part_latch(net, p, "accepted'", s.accepted, picked.accepts);
    // The stepper's own ports, under its name without the trailing space.
    char own[CIRCUIT_NAME];
    copy_name(own, step);
    size_t n = strlen(own);
    if (n > 0) own[n - 1] = '\0';
    net_port(net, own, "hb'", out.hb);
    net_port(net, own, "lb'", out.lb);
    net_port(net, own, "halted'", out.halted);
    net_port(net, own, "accepted'", out.accepted);
    return out;
}

CircuitState part_core(Net *net, const char *prefix, CircuitState start, const int prog[12]) {
    CircuitState s = start;
    for (int k = 1; k <= 6; k++) s = part_stepper(net, prefix, k, s, prog[2 * (k - 1)], prog[2 * (k - 1) + 1]);
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
    CircuitState start;
    start.hb = net_input(net, "in_hb");
    start.lb = net_input(net, "in_lb");
    start.halted = WIRE_0;
    start.accepted = WIRE_0;
    int prog[12];
    program_inputs(net, prog);
    CircuitState end = part_core(net, "", start, prog);
    net_port(net, "", "accepted", end.accepted);
}

void circuit_decides(Net *net) {
    net_init(net);
    int want[4];
    char name[CIRCUIT_NAME];
    for (int n = 0; n < 4; n++) {
        snprintf(name, sizeof name, "want_%d", n);
        want[n] = net_input(net, name);
    }
    int prog[12];
    program_inputs(net, prog);
    int all = -1;
    for (int n = 0; n < 4; n++) {
        char prefix[CIRCUIT_NAME];
        snprintf(prefix, sizeof prefix, "core %d ", n);
        CircuitState start = {n / 2 ? WIRE_1 : WIRE_0, n % 2 ? WIRE_1 : WIRE_0, WIRE_0, WIRE_0};
        CircuitState end = part_core(net, prefix, start, prog);
        // same? is (a AND w) OR (NOT a AND NOT w).
        char part[CIRCUIT_NAME];
        snprintf(part, sizeof part, "same %d", n);
        int a = end.accepted;
        int both = net_and(net, part, "both 1", a, want[n]);
        int not_a = net_not(net, part, "~accepted", a);
        int not_w = net_not(net, part, "~want", want[n]);
        int neither = net_and(net, part, "both 0", not_a, not_w);
        int same = net_or(net, part, "same?", both, neither);
        all = all < 0 ? same : net_and(net, "all", "decides", all, same);
    }
    net_port(net, "", "decides", all);
}

// ── Drawing ─────────────────────────────────────────────────────────────

static const char *kind_word(WireKind k) {
    switch (k) {
    case W_NOT: return "NOT";
    case W_AND: return "AND";
    case W_OR: return "OR";
    default: return "";
    }
}

// A name as a Graphviz label: a quote or a backslash escaped.
static void dot_text(FILE *f, const char *s) {
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') fputc('\\', f);
        fputc(*s, f);
    }
}

void net_dot(const Net *net, const char *part, const char *rankdir, FILE *f) {
    static char drawn[CIRCUIT_MAX_WIRES];
    static char outside[CIRCUIT_MAX_WIRES];
    memset(drawn, 0, sizeof drawn);
    memset(outside, 0, sizeof outside);
    for (int i = 0; i < net->num_wires; i++) {
        const Wire *w = &net->wires[i];
        if (is_gate(w->kind) && in_scope(w->part, part)) drawn[i] = 1;
    }
    for (int i = 0; i < net->num_wires; i++) {
        if (!drawn[i]) continue;
        const Wire *w = &net->wires[i];
        if (!drawn[w->a]) outside[w->a] = 1;
        if (w->kind != W_NOT && !drawn[w->b]) outside[w->b] = 1;
    }
    for (int i = 0; i < net->num_ports; i++) {
        const Port *p = &net->ports[i];
        if (strcmp(p->part, part) == 0 && !drawn[p->wire]) outside[p->wire] = 1;
    }
    fprintf(f, "digraph {\n  rankdir=%s;\n  node [shape=box, style=rounded];\n", rankdir);
    fprintf(f, "  { rank=source;");
    for (int i = 0; i < net->num_wires; i++) {
        if (outside[i]) fprintf(f, " w%d;", i);
    }
    fprintf(f, " }\n");
    for (int i = 0; i < net->num_wires; i++) {
        if (!outside[i]) continue;
        fprintf(f, "  w%d [shape=plaintext, label=\"", i);
        dot_text(f, net->wires[i].name);
        fprintf(f, "\"];\n");
    }
    for (int i = 0; i < net->num_wires; i++) {
        if (!drawn[i]) continue;
        fprintf(f, "  w%d [label=\"%s\\n", i, kind_word(net->wires[i].kind));
        dot_text(f, net->wires[i].name);
        fprintf(f, "\"];\n");
    }
    fprintf(f, "  { rank=sink;");
    for (int i = 0; i < net->num_ports; i++) {
        if (strcmp(net->ports[i].part, part) == 0) fprintf(f, " p%d;", i);
    }
    fprintf(f, " }\n");
    for (int i = 0; i < net->num_ports; i++) {
        if (strcmp(net->ports[i].part, part) != 0) continue;
        fprintf(f, "  p%d [shape=plaintext, label=\"", i);
        dot_text(f, net->ports[i].name);
        fprintf(f, "\"];\n");
    }
    for (int i = 0; i < net->num_wires; i++) {
        if (!drawn[i]) continue;
        const Wire *w = &net->wires[i];
        fprintf(f, "  w%d -> w%d;\n", w->a, i);
        if (w->kind != W_NOT) fprintf(f, "  w%d -> w%d;\n", w->b, i);
    }
    for (int i = 0; i < net->num_ports; i++) {
        if (strcmp(net->ports[i].part, part) == 0) fprintf(f, "  w%d -> p%d;\n", net->ports[i].wire, i);
    }
    fprintf(f, "}\n");
}
