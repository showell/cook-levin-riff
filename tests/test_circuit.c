// test_circuit.c: the netlist of circuit.h, held to the machine. Each part
// is checked on every input it can be given; the stepper on all 64
// settings of its six inputs against sat_step (sat.c), Steve's stepper as
// gates, which keeps running after an accept; a core on all 16,384 runs
// (4,096 programs by four inputs), step by step against sat_step and at the
// end against run_progam; and the decider on every program and every one
// of the 16 want registers against the language the machine says the
// program decides.

#include "test.h"

#include "../src/vm.c"
#include "../src/sat.c"
#include "../src/circuit.c"

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

static Net NET;
static int VALUES[CIRCUIT_MAX_WIRES];

static int port(const char *part, const char *name) { return VALUES[net_port_wire(&NET, part, name)]; }

static void eval(const int *inputs) { net_eval(&NET, inputs, VALUES); }

// A net of one part on four inputs named is_0 to is_3.
static OneHot four_inputs(const char *const names[4]) {
    net_init(&NET);
    OneHot v;
    for (int k = 0; k < 4; k++) v.w[k] = net_input(&NET, names[k]);
    return v;
}

static const char *const NEXT_NAMES[4] = {"next_0", "next_1", "next_2", "next_3"};

TEST(the_decoder_lights_one_wire_for_each_value) {
    net_init(&NET);
    int hb = net_input(&NET, "hb"), lb = net_input(&NET, "lb");
    part_decoder(&NET, "decoder", hb, lb, IS);
    for (int x = 0; x < 4; x++) {
        int in[2] = {x / 2, x % 2};
        eval(in);
        for (int k = 0; k < 4; k++) EXPECT_EQ(port("decoder", IS[k]), k == x);
    }
}

TEST(the_encoder_gives_the_bits_of_the_lit_wire) {
    OneHot v = four_inputs(NEXT_NAMES);
    int bits[2];
    part_encoder(&NET, "encoder", v, bits);
    for (int x = 0; x < 4; x++) {
        int in[4] = {x == 0, x == 1, x == 2, x == 3};
        eval(in);
        EXPECT_EQ(port("encoder", "hb'"), x / 2);
        EXPECT_EQ(port("encoder", "lb'"), x % 2);
    }
    int none[4] = {0, 0, 0, 0};
    eval(none);
    EXPECT_EQ(port("encoder", "hb'") + port("encoder", "lb'"), 0);
}

// Each machine on each value, held to vm_step of a running machine, but
// for `zero`'s halt: Steve's stepper, and so this circuit, leaves halted off
// after an accept.
TEST(each_machine_is_its_opcode_on_every_value) {
    typedef MachineOut (*Build)(Net *, const char *, OneHot);
    static const Build BUILD[4] = {part_nada, part_zero, part_decr, part_mod2};
    static const char *const NAMES[4] = {"nada", "zero", "decr", "mod2"};
    for (int op = 0; op < 4; op++) {
        OneHot is = four_inputs(IS);
        BUILD[op](&NET, NAMES[op], is);
        for (int x = 0; x < 4; x++) {
            int in[4] = {x == 0, x == 1, x == 2, x == 3};
            eval(in);
            Machine m = vm_step(vm_start(x), (Op)op);
            int lit = 0;
            for (int k = 0; k < 4; k++) lit += port(NAMES[op], NEXT_NAMES[k]);
            // A halt leaves AX where it was, and the encoder reads no wire lit as 0.
            if (m.ax == 0) {
                EXPECT(lit <= 1);
                EXPECT_EQ(port(NAMES[op], "next_1") + port(NAMES[op], "next_2") + port(NAMES[op], "next_3"), 0);
            } else {
                EXPECT_EQ(lit, 1);
                EXPECT_EQ(port(NAMES[op], NEXT_NAMES[m.ax]), 1);
            }
            EXPECT_EQ(port(NAMES[op], "accepts"), m.status == STATUS_ACCEPTED);
            EXPECT_EQ(port(NAMES[op], "halts"), m.halted && op != OP_ZERO);
        }
    }
}

TEST(the_hold_makes_a_halted_machine_nada) {
    static const char *const OPS[4] = {"op_nada", "op_zero", "op_decr", "op_mod2"};
    static const char *const EN[4] = {"en_nada", "en_zero", "en_decr", "en_mod2"};
    for (int halted = 0; halted < 2; halted++) {
        OneHot op = four_inputs(OPS);
        int h = net_input(&NET, "halted");
        part_hold(&NET, "hold", op, h);
        for (int x = 0; x < 4; x++) {
            int in[5] = {x == 0, x == 1, x == 2, x == 3, halted};
            eval(in);
            int want = halted ? 0 : x;
            for (int k = 0; k < 4; k++) EXPECT_EQ(port("hold", EN[k]), k == want);
        }
    }
}

// One output wire on every one-hot enable and every setting of its four
// drivers: it is the enabled machine's driver.
TEST(the_selector_passes_the_enabled_machines_wire) {
    net_init(&NET);
    static const char *const EN[4] = {"en_nada", "en_zero", "en_decr", "en_mod2"};
    static const char *const D[4] = {"nada's", "zero's", "decr's", "mod2's"};
    OneHot en;
    int driver[4];
    for (int k = 0; k < 4; k++) en.w[k] = net_input(&NET, EN[k]);
    for (int k = 0; k < 4; k++) driver[k] = net_input(&NET, D[k]);
    part_select_wire(&NET, "selector", "next_1", en, driver);
    EXPECT_EQ(net_count(&NET, "selector").ands, 4);
    EXPECT_EQ(net_count(&NET, "selector").ors, 3);
    for (int e = 0; e < 4; e++) {
        for (int d = 0; d < 16; d++) {
            int in[8] = {e == 0, e == 1, e == 2, e == 3, d & 1, d >> 1 & 1, d >> 2 & 1, d >> 3 & 1};
            eval(in);
            EXPECT_EQ(port("selector", "next_1"), d >> e & 1);
        }
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
    int accepts_without_halting = 0;
    for (int v = 0; v < 64; v++) {
        int ax = v & 3, halted = v >> 2 & 1, accepted = v >> 3 & 1, op = v >> 4;
        int in[6] = {ax / 2, ax % 2, halted, accepted, op / 2, op % 2};
        eval(in);
        SatState next = reference_step((SatState){lit(ax / 2), lit(ax % 2), lit(halted), lit(accepted)}, op);
        EXPECT_EQ(port("", "hb'"), bit(next.hb));
        EXPECT_EQ(port("", "lb'"), bit(next.lb));
        EXPECT_EQ(port("", "halted'"), bit(next.halted));
        EXPECT_EQ(port("", "accepted'"), bit(next.accepted));
        if (!accepted && port("", "accepted'") && !port("", "halted'")) accepts_without_halting++;
    }
    // zero at AX = 0 on a running machine, its accepted flag off: the one
    // setting where vm_step would halt and this stepper runs on.
    EXPECT_EQ(accepts_without_halting, 1);
}

TEST(the_stepper_has_47_gates) {
    circuit_stepper(&NET);
    GateCount all = net_count(&NET, "");
    EXPECT_EQ(all.nots, 5);
    EXPECT_EQ(all.ands, 26);
    EXPECT_EQ(all.ors, 16);
    GateCount sel = net_count(&NET, "selector");
    EXPECT_EQ(sel.ands, 15);
    EXPECT_EQ(sel.ors, 9);
    EXPECT_EQ(net_count(&NET, "nada").ors + net_count(&NET, "zero").ors + net_count(&NET, "decr").ors, 0);
    EXPECT_EQ(net_count(&NET, "mod2").ors, 2);
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
    EXPECT_EQ(all.nots + all.ands + all.ors, 6 * 47);
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
            in[0] = n / 2;
            in[1] = n % 2;
            program_bits(code, in + 2);
            eval(in);
            SatState s = {lit(n / 2), lit(n % 2), SAT_FALSE, SAT_FALSE};
            for (int k = 1; k <= MAX_STEPS; k++) {
                s = reference_step(s, (int)p.ops[k - 1]);
                EXPECT_EQ(VALUES[hb[k]], bit(s.hb));
                EXPECT_EQ(VALUES[lb[k]], bit(s.lb));
                EXPECT_EQ(VALUES[halted[k]], bit(s.halted));
                EXPECT_EQ(VALUES[acc[k]], bit(s.accepted));
            }
            EXPECT_EQ(VALUES[out], run_progam(n, p) == STATUS_ACCEPTED);
        }
    }
}

TEST(the_decider_says_which_problem_each_program_decides) {
    circuit_decides(&NET);
    GateCount all = net_count(&NET, "");
    EXPECT_EQ(all.nots + all.ands + all.ors, 4 * 6 * 47 + 4 * 5 + 3);
    int lit = 0;
    int decides = net_port_wire(&NET, "", "decides");
    for (uint32_t code = 0; code < NUM_PROGRAMS; code++) {
        uint32_t lang = encoded_language(get_language_that_program_accepts(disassemble(code)));
        for (uint32_t want = 0; want < NUM_LANGUAGES; want++) {
            int in[16];
            for (int n = 0; n < 4; n++) in[n] = (int)(want >> n & 1);
            program_bits(code, in + 4);
            eval(in);
            EXPECT_EQ(VALUES[decides], want == lang);
            lit += VALUES[decides];
        }
    }
    EXPECT_EQ(lit, NUM_PROGRAMS);
}

TEST_MAIN(TEST_CASE(the_decoder_lights_one_wire_for_each_value), TEST_CASE(the_encoder_gives_the_bits_of_the_lit_wire),
          TEST_CASE(each_machine_is_its_opcode_on_every_value), TEST_CASE(the_hold_makes_a_halted_machine_nada),
          TEST_CASE(the_selector_passes_the_enabled_machines_wire), TEST_CASE(a_latch_stays_on),
          TEST_CASE(the_stepper_is_sat_step_on_all_64_settings), TEST_CASE(the_stepper_has_47_gates),
          TEST_CASE(a_core_is_the_machine_on_all_16384_runs_step_by_step),
          TEST_CASE(the_decider_says_which_problem_each_program_decides))
