// nibble.c: the machine as the playground's wasm, built by web/build.shale
// with `zig cc -target wasm32-freestanding`. It is src/vm.c itself, with no
// libc (a stop is a trap); a program is its code, the number `assemble`
// gives (op i is base-4 digit i, least significant first).

#define VM_FREESTANDING
#include "../src/vm.c"

#define EXPORT(name) __attribute__((export_name(name)))

// The machine after the first `steps` ops of program `code` on input `n`,
// packed: AX in bits 0-1, halted in bit 2, the Status in bits 3-4.
EXPORT("after") int nibble_after(int n, uint32_t code, int steps) {
    Program program = disassemble(code);
    Machine m = vm_start(n);
    for (int i = 0; i < steps && i < MAX_STEPS; i++) m = vm_step(m, program.ops[i]);
    return m.ax | (m.halted << 2) | ((int)m.status << 3);
}

// run_progam's Status for program `code` on input `n`.
EXPORT("status") int nibble_status(int n, uint32_t code) {
    return (int)run_progam(n, disassemble(code));
}

// The code of the language program `code` accepts.
EXPORT("accepts") int nibble_accepts(uint32_t code) {
    return (int)encoded_language(get_language_that_program_accepts(disassemble(code)));
}
