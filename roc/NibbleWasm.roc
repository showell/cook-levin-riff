# The Nibble page's machine: Machine.roc as the page's wasm, on the platform
# in ../web/platform, which web/build.shale builds and links. Each function
# answers what src/vm.c answers, packed as the page reads it (web/nibble.js),
# and web/page_check.mjs holds all three to the C on every program and input:
#
# - `status(n, code)`: run_progam's Status for program `code` on input `n`,
#   as src/vm.h numbers it (none 0, accepted 1, rejected 2);
# - `after(n, code, k)`: the machine after the program's first `k` ops, AX in
#   bits 0-1, halted in bit 2, the Status in bits 3-4;
# - `accepts(code)`: the code of the language the program accepts.
app [accepts, after, status] { pf: platform "../web/platform/main.roc" }

import Machine

## src/vm.h's Status, as a number.
status_code : Machine.State -> U64
status_code = |machine|
	match machine.ending {
		Running => 0
		Accepted => 1
		Rejected => 2
	}

status : U64, U64 -> U64
status = |input, code| status_code(Machine.run(Machine.disassemble(code), input))

after : U64, U64, U64 -> U64
after = |input, code, steps| {
	machine = Machine.run(Machine.disassemble(code).take_first(steps), input)
	halted : U64
	halted = if (machine.ending == Running) 0 else 1
	machine.ax.bitwise_or(U64.shl_wrap(halted, 2)).bitwise_or(U64.shl_wrap(status_code(machine), 3))
}

accepts : U64 -> U64
accepts = |code| Machine.language_code(Machine.accepted_language(Machine.disassemble(code)))

# The ops of 9 are zero, decr, then nada: input 0 accepts at the first op.
expect status(0, 9) == 1
expect status(1, 9) == 0
expect after(0, 9, 1) == 0 + 4 + 8
# Input 1 passes the zero and is decremented to 0, still running.
expect after(1, 9, 2) == 0
# Program 2 is one decr, which rejects input 0.
expect after(0, 2, 6) == 0 + 4 + 16
expect after(3, 9, 0) == 3
expect accepts(9) == 1
