# The Nibble++ page's machine: Plus.roc as the page's wasm, on the platform
# in ../web/platform/plus.roc, which web/build.shale builds and links. A
# program is its first `lines` lines of `code`, line 1 in bits 0-1, as
# src/plus.h's plus_pack has it, and web/page_check.mjs holds all three
# functions to src/plus.c (web/plus_runs.c's sample):
#
# - `status(n, code, lines)`: how the run on input `n` ends, none 0 (it ran
#   off the end), accepted 1, rejected 2 (a `decr` found AX at 0);
# - `after(n, code, k)`: the machine after the program's first `k` lines, AX
#   in bits 0-3, halted in bit 4, the status in bits 5-6;
# - `accepts(code, lines)`: the language the program decides, input n's bit
#   set when it accepts.
app [accepts, after, status] { pf: platform "../web/platform/plus.roc" }

import Machine
import Plus
import PlusOp

## The longest program a U64 holds, two bits a line (plus.h's
## PLUS_MAX_LINES); a longer `lines`, as the page never asks, reads as this.
max_lines : U64
max_lines = 32

## The program's first `lines` lines.
unpack : U64, U64 -> List(PlusOp)
unpack = |code, lines| {
	n = U64.min(lines, max_lines)
	var $rest = code
	var $ops = List.with_capacity(n)
	for _ in 0..<n {
		$ops = $ops.append(PlusOp.from_code($rest % 4))
		$rest = $rest // 4
	}
	$ops
}

status_code : Machine.State -> U64
status_code = |machine|
	match machine.ending {
		Running => 0
		Accepted => 1
		Rejected => 2
	}

status : U64, U64, U64 -> U64
status = |input, code, lines| status_code(Plus.run(unpack(code, lines), input))

after : U64, U64, U64 -> U64
after = |input, code, steps| {
	machine = Plus.run(unpack(code, steps), input)
	halted : U64
	halted = if (machine.ending == Running) 0 else 1
	machine.ax.bitwise_or(U64.shl_wrap(halted, 4)).bitwise_or(U64.shl_wrap(status_code(machine), 5))
}

accepts : U64, U64 -> U64
accepts = |code, lines| Plus.language_code(unpack(code, lines))

# Line 1 is the low two bits: 9 is zero, decr; 6 is decr, zero.
expect unpack(9, 2) == [PlusOp.Zero, PlusOp.Decr]
expect unpack(9, 4) == [PlusOp.Zero, PlusOp.Decr, PlusOp.Half, PlusOp.Half]
expect accepts(6, 2) == 2
# `half, zero` accepts 0 and 1; `mod2, zero` the even inputs.
expect accepts(0 + 1 * 4, 2) == 3
expect accepts(3 + 1 * 4, 2) == 0x5555
# One decr rejects 0 at line 1; 15 is left as AX 14, still running.
expect after(0, 2, 1) == 0 + 16 + 64
expect after(15, 2, 1) == 14
expect status(15, 2, 1) == 0
# Thirty decrs reject every input: 15 at line 16.
expect accepts(0x0AAAAAAAAAAAAAAA, 30) == 0
expect status(15, 0x0AAAAAAAAAAAAAAA, 30) == 2
expect after(15, 0x0AAAAAAAAAAAAAAA, 15) == 0
expect after(15, 0x0AAAAAAAAAAAAAAA, 16) == 0 + 16 + 64
