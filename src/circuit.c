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
        }
    }
}

// ── The parts ───────────────────────────────────────────────────────────

static const char *const MACHINES[4] = {"nada", "zero", "decr", "mod2"};
static const char *const NEXT[4] = {"next_0", "next_1", "next_2", "next_3"};
static const char *const IS[4] = {"is_0", "is_1", "is_2", "is_3"};
static const char *const OPS[4] = {"op_nada", "op_zero", "op_decr", "op_mod2"};
static const char *const EN[4] = {"en_nada", "en_zero", "en_decr", "en_mod2"};

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

// zero: every wire stays, and is_0 is tapped to accept. The run goes on
// after an accept, as every step of a circuit runs: the accepted latch
// holds the verdict, and halted stays off, as Steve's stepper has it.
MachineOut part_zero(Net *net, const char *part, OneHot is) {
    MachineOut m = {is, is.w[0], WIRE_0};
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
    int running = net_not(net, part, "~halted", halted);
    OneHot en;
    en.w[0] = net_or(net, part, EN[0], op.w[0], halted);
    for (int k = 1; k < 4; k++) en.w[k] = net_and(net, part, EN[k], op.w[k], running);
    for (int k = 0; k < 4; k++) net_port(net, part, EN[k], en.w[k]);
    return en;
}

int part_select_wire(Net *net, const char *part, const char *name, OneHot enable, const int driver[4]) {
    int picked[4];
    int n = 0;
    for (int m = 0; m < 4; m++) {
        if (driver[m] != WIRE_0) picked[n++] = net_and(net, part, MACHINES[m], driver[m], enable.w[m]);
    }
    // The ORs in pairs, a tree as shallow as the count allows.
    while (n > 1) {
        int kept = 0;
        for (int i = 0; i + 1 < n; i += 2) picked[kept++] = net_or(net, part, name, picked[i], picked[i + 1]);
        if (n % 2 == 1) picked[kept++] = picked[n - 1];
        n = kept;
    }
    int out = n == 1 ? picked[0] : WIRE_0;
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

// How a stepper's boxes are wired, as part_stepper wires them.
static void stepper_links(Net *net, const char *own, const MachineOut m[4]) {
    net_link(net, own, "decoder", "hb", "", "hb");
    net_link(net, own, "decoder", "lb", "", "lb");
    net_link(net, own, "op decoder", "op_hb", "", "op_hb");
    net_link(net, own, "op decoder", "op_lb", "", "op_lb");
    for (int k = 0; k < 4; k++) net_link(net, own, "hold", OPS[k], "op decoder", OPS[k]);
    net_link(net, own, "hold", "halted", "", "halted");
    for (int i = 0; i < 4; i++) {
        for (int k = 0; k < 4; k++) net_link(net, own, MACHINES[i], IS[k], "decoder", IS[k]);
    }
    for (int k = 0; k < 4; k++) net_link(net, own, "selector", EN[k], "hold", EN[k]);
    for (int i = 0; i < 4; i++) {
        for (int k = 0; k < 4; k++) {
            if (m[i].next.w[k] != WIRE_0) net_link(net, own, "selector", NEXT[k], MACHINES[i], NEXT[k]);
        }
        if (m[i].accepts != WIRE_0) net_link(net, own, "selector", "accepts", MACHINES[i], "accepts");
        if (m[i].halts != WIRE_0) net_link(net, own, "selector", "halts", MACHINES[i], "halts");
    }
    for (int k = 1; k < 4; k++) net_link(net, own, "encoder", NEXT[k], "selector", NEXT[k]);
    net_link(net, own, "latches", "halted", "", "halted");
    net_link(net, own, "latches", "halts", "selector", "halts");
    net_link(net, own, "latches", "accepted", "", "accepted");
    net_link(net, own, "latches", "accepts", "selector", "accepts");
    net_link(net, own, "", "hb'", "encoder", "hb'");
    net_link(net, own, "", "lb'", "encoder", "lb'");
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
    stepper_links(net, own, m);
    net_port(net, own, "hb'", out.hb);
    net_port(net, own, "lb'", out.lb);
    net_port(net, own, "halted'", out.halted);
    net_port(net, own, "accepted'", out.accepted);
    return out;
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
        decider_links(net, n);
    }
    net_link(net, "", "", "decides", "all", "decides");
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

// The first `words` words of a gate's part past the scope, into `to`.
static void cluster_of(const char *part, const char *scope, int words, char *to) {
    size_t n = strlen(scope);
    const char *s = part + n;
    if (n > 0 && *s == ' ') s++;
    size_t len = 0;
    int seen = 0;
    while (s[len] != '\0') {
        if (s[len] == ' ' && ++seen == words) break;
        len++;
    }
    memcpy(to, s, len);
    to[len] = '\0';
}

static void dot_gate(const Net *net, int i, const char *indent, FILE *f) {
    fprintf(f, "%sw%d [label=\"%s\\n", indent, i, kind_word(net->wires[i].kind));
    dot_text(f, net->wires[i].name);
    fprintf(f, "\"];\n");
}

void net_dot(const Net *net, const char *part, DotStyle style, FILE *f) {
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
    fprintf(f, "digraph {\n  rankdir=%s;\n", style.rankdir);
    fprintf(f, "  node [shape=box, style=rounded];\n");
    if (style.pin_ends) {
        fprintf(f, "  { rank=source;");
        for (int i = 0; i < net->num_wires; i++) {
            if (outside[i]) fprintf(f, " w%d;", i);
        }
        fprintf(f, " }\n");
    }
    for (int i = 0; i < net->num_wires; i++) {
        if (!outside[i]) continue;
        fprintf(f, "  w%d [shape=plaintext, label=\"", i);
        dot_text(f, net->wires[i].name);
        fprintf(f, "\"];\n");
    }
    if (style.cluster_words > 0) {
        static char done[CIRCUIT_MAX_WIRES];
        memset(done, 0, sizeof done);
        int clusters = 0;
        char key[CIRCUIT_NAME], other[CIRCUIT_NAME];
        for (int i = 0; i < net->num_wires; i++) {
            if (!drawn[i] || done[i]) continue;
            cluster_of(net->wires[i].part, part, style.cluster_words, key);
            fprintf(f, "  subgraph cluster_%d {\n    label=\"", ++clusters);
            dot_text(f, key);
            fprintf(f, "\";\n");
            for (int j = i; j < net->num_wires; j++) {
                if (!drawn[j] || done[j]) continue;
                cluster_of(net->wires[j].part, part, style.cluster_words, other);
                if (strcmp(key, other) != 0) continue;
                dot_gate(net, j, "    ", f);
                done[j] = 1;
            }
            fprintf(f, "  }\n");
        }
    } else {
        for (int i = 0; i < net->num_wires; i++) {
            if (drawn[i]) dot_gate(net, i, "  ", f);
        }
    }
    if (style.pin_ends) {
        fprintf(f, "  { rank=sink;");
        for (int i = 0; i < net->num_ports; i++) {
            if (strcmp(net->ports[i].part, part) == 0) fprintf(f, " p%d;", i);
        }
        fprintf(f, " }\n");
    }
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
    fprintf(f, "digraph {\n  rankdir=%s;\n  node [shape=box, style=rounded];\n", rankdir);
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
