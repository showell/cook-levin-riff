// test_vm.c: virtual_machine.py's test() asserts, and the names and
// comments find_solutions prints.

#include "test.h"

#include "../src/vm.c"

TEST(assemble_of_zero_decr_is_9) {
    Op zd[2] = {OP_ZERO, OP_DECR};
    EXPECT_EQ(assemble(zd, 2), 9);
    Op named[2] = {op_from_name("zero"), op_from_name("decr")};
    EXPECT_EQ(assemble(named, 2), 9);
}

TEST(disassemble_of_9_is_zero_decr_then_four_nadas) {
    Program p = disassemble(9);
    Op want[MAX_STEPS] = {OP_ZERO, OP_DECR, OP_NADA, OP_NADA, OP_NADA, OP_NADA};
    for (int i = 0; i < MAX_STEPS; i++) EXPECT_EQ(p.ops[i], want[i]);
}

TEST(the_empty_language_is_code_0) {
    Lang empty = {{0}, 0};
    EXPECT_EQ(encoded_language(empty), 0);
    EXPECT_EQ(language(0).len, 0);
}

TEST(the_odd_inputs_are_code_10) {
    Lang odd = {{1, 3}, 2};
    EXPECT_EQ(encoded_language(odd), 10);
    Lang ten = language(10);
    EXPECT_EQ(ten.len, 2);
    EXPECT_EQ(ten.items[0], 1);
    EXPECT_EQ(ten.items[1], 3);
}

TEST(vm_test_passes_as_pythons_test_does) {
    vm_test();
}

TEST(assemble_and_disassemble_are_inverses_on_every_program) {
    for (uint32_t n = 0; n < NUM_PROGRAMS; n++) {
        Program p = disassemble(n);
        EXPECT_EQ(assemble(p.ops, MAX_STEPS), n);
    }
}

TEST(every_code_round_trips_through_its_language_and_complement) {
    for (uint32_t code = 0; code < NUM_LANGUAGES; code++) {
        Lang lang = language(code);
        EXPECT_EQ(encoded_language(lang), code);
        EXPECT_EQ(encoded_language(complement(lang)), 15 - code);
        for (size_t i = 1; i < lang.len; i++) EXPECT(lang.items[i - 1] < lang.items[i]);
    }
}

TEST(each_op_has_its_name_and_comment) {
    EXPECT(strcmp(op_name(OP_NADA), "nada") == 0);
    EXPECT(strcmp(op_name(OP_ZERO), "zero") == 0);
    EXPECT(strcmp(op_name(OP_DECR), "decr") == 0);
    EXPECT(strcmp(op_name(OP_MOD2), "mod2") == 0);
    EXPECT(strcmp(op_comment(OP_NADA), "# do nothing") == 0);
    EXPECT(strcmp(op_comment(OP_ZERO), "# accept original input if AX = 0") == 0);
    EXPECT(strcmp(op_comment(OP_DECR), "# reject zero or decrement AX") == 0);
    EXPECT(strcmp(op_comment(OP_MOD2), "# subtract 2 from AX if AX >= 2") == 0);
    for (int i = 0; i < NUM_OPS; i++) EXPECT_EQ(op_from_name(op_name((Op)i)), i);
}

TEST_MAIN(TEST_CASE(assemble_of_zero_decr_is_9), TEST_CASE(disassemble_of_9_is_zero_decr_then_four_nadas),
          TEST_CASE(the_empty_language_is_code_0), TEST_CASE(the_odd_inputs_are_code_10),
          TEST_CASE(vm_test_passes_as_pythons_test_does),
          TEST_CASE(assemble_and_disassemble_are_inverses_on_every_program),
          TEST_CASE(every_code_round_trips_through_its_language_and_complement),
          TEST_CASE(each_op_has_its_name_and_comment))
