// circuit.h: the machine as a circuit of AND, OR and NOT gates, a netlist
// with a simulator, for the book's circuit chapters.
//
// A netlist is a list of wires in the order they were made, so every gate's
// inputs come before it and one pass in order evaluates it. A wire is an
// input, a constant, or a gate's output. Each gate carries a name and the
// name of the part that made it (`step 3 hb mux low`), each part's outputs
// are named ports, and each composed part records how its boxes are wired,
// so a printer can draw any part alone, at gate level or as boxes.
//
// AX travels as its two bits, hb and lb. The parts, each a literal
// transcription of its rule:
//
//   machines   one per opcode, from (hb, lb) to (hb', lb') and a flag:
//              nada passes both bits; mod2 ties hb' to 0 and passes lb;
//              zero passes both and accepts on NOR(hb, lb); decr is the
//              two-bit decrementer, hb' = hb AND lb, lb' = NOT lb, and
//              halts on NOR(hb, lb). A NOR is an OR and then a NOT.
//   mux2       a 2-way multiplexer: (in 0 AND NOT s) OR (in 1 AND s).
//   mux4       a 4-way multiplexer of three mux2s: s_lb picks within the
//              pairs (in 0, in 1) and (in 2, in 3), s_hb between them.
//   hold       a halted machine runs nada: the select bits AND NOT halted.
//   latch      a flag OR the step's new flag, so a flag once on stays on.
//   stepper    the four machines on AX, the hold on the line's bits, one
//              mux4 for each of hb', lb', accepts and halts, selected by
//              the held line, and the two latches.
//   core       six steppers in a chain from AX = the input, flags 0.
//   decides    four cores, one per input, each compared with a want bit,
//              and an AND of the four comparisons.
//
// The stepper follows Steve's stepper (stepper.py, and sat_step in sat.c):
// `zero` at AX = 0 accepts and the run goes on, since a circuit runs every
// step; `decr` at AX = 0 halts; a halted machine stays as it is. The one
// difference is AX after `decr` halts at 0: the decrementer leaves 1 there,
// where sat_step leaves 0, and the halted machine keeps it. The flags are
// sat_step's on every setting, and the final accepted is the interpreter's
// on every run.

#ifndef COOK_LEVIN_CIRCUIT_H
#define COOK_LEVIN_CIRCUIT_H

#include <stdio.h>

enum { CIRCUIT_MAX_WIRES = 1600, CIRCUIT_MAX_PORTS = 1400, CIRCUIT_MAX_LINKS = 2400, CIRCUIT_NAME = 40 };

// W_ALIAS is a named copy of another wire, no gate: a core's input, wired
// to a constant inside the decider.
typedef enum { W_ZERO, W_ONE, W_INPUT, W_NOT, W_AND, W_OR, W_ALIAS } WireKind;

typedef struct {
    WireKind kind;
    int a, b;
    char name[CIRCUIT_NAME];
    char part[CIRCUIT_NAME];
} Wire;

// A part's output: a name for a wire, which may be a gate of the part or a
// wire the part passes through.
typedef struct {
    char part[CIRCUIT_NAME];
    char name[CIRCUIT_NAME];
    int wire;
} Port;

// One wire from one box to another inside a composed part, `scope`: the
// part a composer (part_stepper, part_core, circuit_decides) builds from
// boxes. `to` and `from` are the boxes' names within the scope ("decoder",
// "step 3"); "" is the scope itself, so a link from "" is one of the scope's
// inputs, named `from_name`, and a link to "" is one of its outputs, named
// `to_name`. The drawing of a scope's boxes reads these alone.
typedef struct {
    char scope[CIRCUIT_NAME];
    char to[CIRCUIT_NAME];
    char to_name[CIRCUIT_NAME];
    char from[CIRCUIT_NAME];
    char from_name[CIRCUIT_NAME];
} Link;

typedef struct {
    Wire wires[CIRCUIT_MAX_WIRES];
    int num_wires;
    Port ports[CIRCUIT_MAX_PORTS];
    int num_ports;
    Link links[CIRCUIT_MAX_LINKS];
    int num_links;
} Net;

// The wires 0 and 1 of every net are the constants 0 and 1.
enum { WIRE_0 = 0, WIRE_1 = 1 };

// A new net, holding the two constants. A Net is large: keep it static.
void net_init(Net *net);
int net_input(Net *net, const char *name);
int net_alias(Net *net, const char *part, const char *name, int a);
int net_not(Net *net, const char *part, const char *name, int a);
int net_and(Net *net, const char *part, const char *name, int a, int b);
int net_or(Net *net, const char *part, const char *name, int a, int b);
void net_port(Net *net, const char *part, const char *name, int wire);
void net_link(Net *net, const char *scope, const char *to, const char *to_name, const char *from, const char *from_name);

// How many gates of each kind a part made: every part whose name is `part`
// or starts with `part` and a space ("step 1" holds "step 1 decoder");
// "" counts the whole net.
// Padded to 16 bytes past tcc's struct-return bug (stepper.h's StepState).
typedef struct {
    int nots, ands, ors, unused;
} GateCount;
_Static_assert(sizeof(GateCount) == 16, "GateCount is 16 bytes (see stepper.h)");
GateCount net_count(const Net *net, const char *part);

// Every wire's value, given the inputs' values in the order they were made.
void net_eval(const Net *net, const int *inputs, int *values);

// ── The parts ───────────────────────────────────────────────────────────

// What an opcode machine gives: AX's next two bits, and the two flags, the
// constant 0 where the machine has none.
typedef struct {
    int hb, lb, accepts, halts;
} MachineOut;

MachineOut part_nada(Net *net, const char *part, int hb, int lb);
MachineOut part_zero(Net *net, const char *part, int hb, int lb);
MachineOut part_decr(Net *net, const char *part, int hb, int lb);
MachineOut part_mod2(Net *net, const char *part, int hb, int lb);
int part_mux2(Net *net, const char *part, int s, int in0, int in1);
// in[k] is chosen when the select bits s_hb, s_lb spell k.
int part_mux4(Net *net, const char *part, int s_hb, int s_lb, const int in[4]);
// Gives the held select bits, sel[0] hb's and sel[1] lb's.
void part_hold(Net *net, const char *part, int op_hb, int op_lb, int halted, int sel[2]);
int part_latch(Net *net, const char *part, const char *name, int flag, int news);

// A state between steps: AX's two bits and the two flags.
typedef struct {
    int hb, lb, halted, accepted;
} CircuitState;

// Step `k` (1 to 6) of a core, its parts named "<prefix>step k ...".
CircuitState part_stepper(Net *net, const char *prefix, int k, CircuitState s, int op_hb, int op_lb);
// A core: six steppers from `start`, the program's twelve bits as
// prog[2i] (line i+1's hb) and prog[2i+1] (its lb). Gives the last state.
CircuitState part_core(Net *net, const char *prefix, CircuitState start, const int prog[12]);

// The stepper alone, on six inputs made in the order hb, lb, halted,
// accepted, op_hb, op_lb, its ports named hb', lb', halted', accepted'.
void circuit_stepper(Net *net);
// A core alone, on fourteen inputs: the program's twelve bits, line 1's
// first, then AX's two; its one port is accepted.
void circuit_core(Net *net);
// The whole decider, on sixteen inputs: the program's twelve bits, line 1's
// first, then the four want bits (input 0's first); its one port is decides.
void circuit_decides(Net *net);

// The wire of the port named `name` of part `part`.
int net_port_wire(const Net *net, const char *part, const char *name);

// A schematic of the gates of `part`, as SVG: AND as a D, OR as a shield,
// NOT as a triangle with a bubble, and an OR read only by a NOT drawn as one
// NOR. The net's inputs, in the order they were made, stand on the left
// with the constant 0 when a gate or port reads it, and the part's ports on
// the right; a dot marks each place a wire branches. Columns are the gates'
// depths, so every wire runs left to right. The strokes and text take the
// page's colour (currentColor).
void net_svg(const Net *net, const char *part, FILE *f);

// The net as Boolean equations, one line a gate in the order they are made,
// so each reads only names defined above it: first a line `inputs: ...`
// naming the inputs, then a line `name = value` for each alias, then
// `name = a AND b`, `name = a OR b` or `name = NOT a` for each gate. A
// wire's name is its outermost port's, the part's words joined with dots
// (`core 2 step 3 decr` is `core2.step3.decr`, a number joined to the word
// before it, and `hb mux` is `hb_mux`), then the port's name:
// `core2.step3.hb'`. A wire no port names is its gate's own name in its
// part, `core2.step3.hb_mux.low.pick0`. The constants are 0 and 1. A name
// made twice stops the program.
void net_equations(const Net *net, FILE *f);

// A Graphviz drawing of a composed part as its boxes, from the links whose
// scope is `scope`: each box a node named for its part, each input and
// output of the scope a named end, and one line for each pair of ends that
// wires join, marked with how many wires it carries when that is more
// than one.
void net_boxes_dot(const Net *net, const char *scope, const char *rankdir, FILE *f);

#endif
