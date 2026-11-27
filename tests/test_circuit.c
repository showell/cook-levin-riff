// test_circuit.c: the netlist of circuit.h, held to the machine. Each part
// is checked on every input it can be given; the stepper on all 64
// settings of its six inputs against sat_step (sat.c), Steve's stepper as
// gates, which keeps running after an accept; a core on all 16,384 runs
// (4,096 programs by four inputs), step by step against sat_step and at the
// end against run_progam; and the decider on every program and every one
// of the 16 want registers against the language the machine says the
// program decides.
//
// The flags are sat_step's everywhere. AX is too, but where `decr` halts
// at 0: the decrementer leaves 1 there, where sat_step leaves 0, and the
// halted machine keeps it. So AX is held to sat_step's while the machine
// runs, and the halting setting is checked to give 1.

#include "test.h"

#include "../src/vm.c"
#include "../src/sat.c"
#include "../src/circuit.c"

static Net NET;
static int VALUES[CIRCUIT_MAX_WIRES];

static int port(const char *part, const char *name) { return VALUES[net_port_wire(&NET, part, name)]; }

static void eval(const int *inputs) { net_eval(&NET, inputs, VALUES); }

// sat_step on constant wires: the step as Steve's stepper takes it, with no
// gate made.
static SatState reference_step(SatState s, int op) {
    Cnf c = sat_new();
    SatState out = sat_step(&c, s, op / 2 ? SAT_TRUE : SAT_FALSE, op % 2 ? SAT_TRUE : SAT_FALSE);
    sat_free(&c);
    return out;
}

static Lit lit(int bit) { return bit ? SAT_TRUE : SAT_FALSE; }

static int bit(Lit x) {
    if (x != SAT_TRUE && x != SAT_FALSE) {
        fprintf(stderr, "  literal %d is no constant\n", x);
        abort();
    }
    return x == SAT_TRUE;
}

static int has_port(const char *part, const char *name) {
    for (int i = 0; i < NET.num_ports; i++) {
        if (strcmp(NET.ports[i].part, part) == 0 && strcmp(NET.ports[i].name, name) == 0) return 1;
    }
    return 0;
}

// Each machine on each value of AX: vm_step's AX and accept for a running
// machine, a halt only from decr at 0, and AX 1 there, the decrementer's.
TEST(each_machine_is_its_opcode_on_every_value) {
    typedef MachineOut (*Build)(Net *, const char *, int, int);
    static const Build BUILD[4] = {part_nada, part_zero, part_decr, part_mod2};
    for (int op = 0; op < 4; op++) {
        net_init(&NET);
        int hb = net_input(&NET, "hb"), lb = net_input(&NET, "lb");
        BUILD[op](&NET, MACHINES[op], hb, lb);
        for (int x = 0; x < 4; x++) {
            int in[2] = {x / 2, x % 2};
            eval(in);
            Machine m = vm_step(vm_start(x), (Op)op);
            int ax = 2 * port(MACHINES[op], "hb'") + port(MACHINES[op], "lb'");
            int halts = op == OP_DECR && x == 0;
            EXPECT_EQ(ax, halts ? 1 : m.ax);
            EXPECT_EQ(has_port(MACHINES[op], "accepts") ? port(MACHINES[op], "accepts") : 0, m.status == STATUS_ACCEPTED);
            EXPECT_EQ(has_port(MACHINES[op], "halts") ? port(MACHINES[op], "halts") : 0, halts);
        }
    }
}

TEST(a_mux2_passes_the_input_its_select_names) {
    net_init(&NET);
    int s = net_input(&NET, "s"), a = net_input(&NET, "in 0"), b = net_input(&NET, "in 1");
    part_mux2(&NET, "mux2", s, a, b);
    for (int v = 0; v < 8; v++) {
        int in[3] = {v & 1, v >> 1 & 1, v >> 2 & 1};
        eval(in);
        EXPECT_EQ(port("mux2", "out"), in[0] ? in[2] : in[1]);
    }
}

TEST(a_mux4_passes_the_input_its_selects_name) {
    net_init(&NET);
    int s_hb = net_input(&NET, "s_hb"), s_lb = net_input(&NET, "s_lb");
    int d[4];
    char name[8];
    for (int k = 0; k < 4; k++) {
        snprintf(name, sizeof name, "in %d", k);
        d[k] = net_input(&NET, name);
    }
    part_mux4(&NET, "mux4", s_hb, s_lb, d);
    for (int v = 0; v < 64; v++) {
        int in[6] = {v & 1, v >> 1 & 1, v >> 2 & 1, v >> 3 & 1, v >> 4 & 1, v >> 5 & 1};
        eval(in);
        EXPECT_EQ(port("mux4", "out"), in[2 + 2 * in[0] + in[1]]);
    }
    GateCount c = net_count(&NET, "mux4");
    EXPECT_EQ(c.nots + c.ands + c.ors, 12);
}

TEST(the_hold_makes_a_halted_machine_nada) {
    net_init(&NET);
    int op_hb = net_input(&NET, "op_hb"), op_lb = net_input(&NET, "op_lb"), h = net_input(&NET, "halted");
    int sel[2];
    part_hold(&NET, "hold", op_hb, op_lb, h, sel);
    for (int v = 0; v < 8; v++) {
        int in[3] = {v & 1, v >> 1 & 1, v >> 2 & 1};
        eval(in);
        EXPECT_EQ(port("hold", "sel_hb"), in[2] ? 0 : in[0]);
        EXPECT_EQ(port("hold", "sel_lb"), in[2] ? 0 : in[1]);
    }
}

TEST(a_latch_stays_on) {
    net_init(&NET);
    int flag = net_input(&NET, "accepted"), news = net_input(&NET, "accepts");
    part_latch(&NET, "latches", "accepted'", flag, news);
    for (int v = 0; v < 4; v++) {
        int in[2] = {v & 1, v >> 1};
        eval(in);
        EXPECT_EQ(port("latches", "accepted'"), (v & 1) | (v >> 1));
    }
}

TEST(the_stepper_is_sat_step_on_all_64_settings) {
    circuit_stepper(&NET);
    int halting = 0, accepts_without_halting = 0;
    for (int v = 0; v < 64; v++) {
        int ax = v & 3, halted = v >> 2 & 1, accepted = v >> 3 & 1, op = v >> 4;
        int in[6] = {ax / 2, ax % 2, halted, accepted, op / 2, op % 2};
        eval(in);
        SatState next = reference_step((SatState){lit(ax / 2), lit(ax % 2), lit(halted), lit(accepted)}, op);
        EXPECT_EQ(port("", "halted'"), bit(next.halted));
        EXPECT_EQ(port("", "accepted'"), bit(next.accepted));
        int out_ax = 2 * port("", "hb'") + port("", "lb'");
        if (!halted && bit(next.halted)) {
            // decr at 0: the decrementer's 1, where sat_step keeps 0.
            EXPECT_EQ(op, OP_DECR);
            EXPECT_EQ(out_ax, 1);
            halting++;
        } else {
            EXPECT_EQ(out_ax, 2 * bit(next.hb) + bit(next.lb));
        }
        if (!accepted && port("", "accepted'") && !port("", "halted'")) accepts_without_halting++;
    }
    // decr at 0 on a running machine, accepted 0 or 1.
    EXPECT_EQ(halting, 2);
    // zero at AX = 0 on a running machine, its accepted flag off: the one
    // setting where vm_step would halt and this stepper runs on.
    EXPECT_EQ(accepts_without_halting, 1);
}

TEST(the_stepper_has_59_gates) {
    circuit_stepper(&NET);
    GateCount all = net_count(&NET, "");
    EXPECT_EQ(all.nots, 1 + 2 + 1 + 4 * 3);
    EXPECT_EQ(all.ands, 1 + 2 + 4 * 6);
    EXPECT_EQ(all.ors, 2 + 4 * 3 + 2);
    EXPECT_EQ(all.nots + all.ands + all.ors, 59);
    GateCount zero = net_count(&NET, "zero");
    EXPECT_EQ(zero.nots + zero.ands + zero.ors, 2);
    GateCount decr = net_count(&NET, "decr");
    EXPECT_EQ(decr.nots + decr.ands + decr.ors, 4);
    GateCount nada = net_count(&NET, "nada"), mod2 = net_count(&NET, "mod2");
    EXPECT_EQ(nada.nots + nada.ands + nada.ors + mod2.nots + mod2.ands + mod2.ors, 0);
}

static void program_bits(uint32_t code, int *in) {
    Program p = disassemble(code);
    for (int i = 0; i < MAX_STEPS; i++) {
        in[2 * i] = (int)p.ops[i] / 2;
        in[2 * i + 1] = (int)p.ops[i] % 2;
    }
}

TEST(a_core_is_the_machine_on_all_16384_runs_step_by_step) {
    circuit_core(&NET);
    GateCount all = net_count(&NET, "");
    EXPECT_EQ(all.nots + all.ands + all.ors, 6 * 59);
    char part[16];
    int hb[MAX_STEPS + 1], lb[MAX_STEPS + 1], halted[MAX_STEPS + 1], acc[MAX_STEPS + 1];
    for (int k = 1; k <= MAX_STEPS; k++) {
        snprintf(part, sizeof part, "step %d", k);
        hb[k] = net_port_wire(&NET, part, "hb'");
        lb[k] = net_port_wire(&NET, part, "lb'");
        halted[k] = net_port_wire(&NET, part, "halted'");
        acc[k] = net_port_wire(&NET, part, "accepted'");
    }
    int out = net_port_wire(&NET, "", "accepted");
    for (uint32_t code = 0; code < NUM_PROGRAMS; code++) {
        Program p = disassemble(code);
        for (int n = 0; n < NUM_INPUTS; n++) {
            int in[14];
            program_bits(code, in);
            in[12] = n / 2;
            in[13] = n % 2;
            eval(in);
            SatState s = {lit(n / 2), lit(n % 2), SAT_FALSE, SAT_FALSE};
            for (int k = 1; k <= MAX_STEPS; k++) {
                s = reference_step(s, (int)p.ops[k - 1]);
                EXPECT_EQ(VALUES[halted[k]], bit(s.halted));
                EXPECT_EQ(VALUES[acc[k]], bit(s.accepted));
                if (!bit(s.halted)) {
                    EXPECT_EQ(VALUES[hb[k]], bit(s.hb));
                    EXPECT_EQ(VALUES[lb[k]], bit(s.lb));
                }
            }
            EXPECT_EQ(VALUES[out], run_progam(n, p) == STATUS_ACCEPTED);
        }
    }
}

TEST(the_decider_says_which_problem_each_program_decides) {
    circuit_decides(&NET);
    GateCount all = net_count(&NET, "");
    EXPECT_EQ(all.nots + all.ands + all.ors, 4 * 6 * 59 + 4 * 5 + 3);
    int lit_count = 0;
    int decides = net_port_wire(&NET, "", "decides");
    for (uint32_t code = 0; code < NUM_PROGRAMS; code++) {
        uint32_t lang = encoded_language(get_language_that_program_accepts(disassemble(code)));
        for (uint32_t want = 0; want < NUM_LANGUAGES; want++) {
            int in[16];
            program_bits(code, in);
            for (int n = 0; n < 4; n++) in[12 + n] = (int)(want >> n & 1);
            eval(in);
            EXPECT_EQ(VALUES[decides], want == lang);
            lit_count += VALUES[decides];
        }
    }
    EXPECT_EQ(lit_count, NUM_PROGRAMS);
}

TEST_MAIN(TEST_CASE(each_machine_is_its_opcode_on_every_value), TEST_CASE(a_mux2_passes_the_input_its_select_names),
          TEST_CASE(a_mux4_passes_the_input_its_selects_name), TEST_CASE(the_hold_makes_a_halted_machine_nada),
          TEST_CASE(a_latch_stays_on), TEST_CASE(the_stepper_is_sat_step_on_all_64_settings),
          TEST_CASE(the_stepper_has_59_gates), TEST_CASE(a_core_is_the_machine_on_all_16384_runs_step_by_step),
          TEST_CASE(the_decider_says_which_problem_each_program_decides))
