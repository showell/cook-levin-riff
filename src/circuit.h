// circuit.h: the machine as a circuit of AND, OR and NOT gates, a netlist
// with a simulator, for the book's circuit chapters.
//
// A netlist is a list of wires in the order they were made, so every gate's
// inputs come before it and one pass in order evaluates it. A wire is an
// input, a constant, or a gate's output. Each gate carries a name (`is_0`)
// and the name of the part that made it (`step 3 decoder`), and each part's
// outputs are named ports, so a printer can draw any part alone or the
// whole circuit from the same list.
//
// The parts are built bottom-up, each a literal transcription of its rule:
//
//   decoder    2 bits to four one-hot wires, is_0 to is_3.
//   encoder    one-hot wires 1 to 3 back to 2 bits.
//   machines   one per opcode, on the one-hot wires: what AX becomes, as
//              four one-hot wires, and whether the step accepts or halts.
//              nada, zero and decr are wires alone; mod2 folds with ORs.
//   hold       the op decoder's four wires and `halted` to four enables: a
//              halted machine acts as nada.
//   selector   each output wire is the OR over the machines of that
//              machine's wire AND its enable.
//   latch      a flag OR the step's new flag, so a flag once on stays on.
//   stepper    decoder, op decoder, hold, the four machines, selector,
//              encoder and two latches.
//   core       six steppers in a chain from AX = the input, flags 0.
//   decides    four cores, one per input, each compared with a want bit,
//              and an AND of the four comparisons.
//
// The stepper follows vm_step (vm.c): `zero` at AX = 0 accepts and halts,
// `decr` at AX = 0 halts, and a halted machine stays as it is.

#ifndef COOK_LEVIN_CIRCUIT_H
#define COOK_LEVIN_CIRCUIT_H

#include <stdio.h>

enum { CIRCUIT_MAX_WIRES = 1600, CIRCUIT_MAX_PORTS = 1400, CIRCUIT_NAME = 40 };

typedef enum { W_ZERO, W_ONE, W_INPUT, W_NOT, W_AND, W_OR } WireKind;

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

typedef struct {
    Wire wires[CIRCUIT_MAX_WIRES];
    int num_wires;
    Port ports[CIRCUIT_MAX_PORTS];
    int num_ports;
} Net;

// The wires 0 and 1 of every net are the constants 0 and 1.
enum { WIRE_0 = 0, WIRE_1 = 1 };

// A new net, holding the two constants. A Net is large: keep it static.
void net_init(Net *net);
int net_input(Net *net, const char *name);
int net_not(Net *net, const char *part, const char *name, int a);
int net_and(Net *net, const char *part, const char *name, int a, int b);
int net_or(Net *net, const char *part, const char *name, int a, int b);
void net_port(Net *net, const char *part, const char *name, int wire);

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

// Four one-hot wires, a value 0 to 3.
typedef struct {
    int w[4];
} OneHot;

// What an opcode machine gives: the next value one-hot, and the two flags.
typedef struct {
    OneHot next;
    int accepts, halts;
} MachineOut;

OneHot part_decoder(Net *net, const char *part, int hb, int lb, const char *const names[4]);
// Gives the two bits; `bits[0]` is hb, `bits[1]` lb.
void part_encoder(Net *net, const char *part, OneHot v, int bits[2]);
MachineOut part_nada(Net *net, const char *part, OneHot is);
MachineOut part_zero(Net *net, const char *part, OneHot is);
MachineOut part_decr(Net *net, const char *part, OneHot is);
MachineOut part_mod2(Net *net, const char *part, OneHot is);
OneHot part_hold(Net *net, const char *part, OneHot op, int halted);
// One output wire of the selector: the OR over the machines of driver[m]
// AND enable[m], a driver that is the constant 0 left out.
int part_select_wire(Net *net, const char *part, const char *name, OneHot enable, const int driver[4]);
MachineOut part_selector(Net *net, const char *part, OneHot enable, const MachineOut m[4]);
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
// A core alone, on fourteen inputs: AX's two bits, then the program's
// twelve, line 1's first; its one port is accepted.
void circuit_core(Net *net);
// The whole decider, on sixteen inputs: the four want bits (input 0's
// first), then the program's twelve; its one port is decides.
void circuit_decides(Net *net);

// The wire of the port named `name` of part `part`.
int net_port_wire(const Net *net, const char *part, const char *name);

// A Graphviz drawing of the gates of `part` (as net_count reads it), with
// the wires they read from outside as inputs and the part's own ports as
// outputs. `rankdir` is "LR" or "TB".
void net_dot(const Net *net, const char *part, const char *rankdir, FILE *f);

#endif
