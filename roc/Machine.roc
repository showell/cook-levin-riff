## The machine of Steve's virtual_machine.py, as src/vm.c has it: one 2-bit
## register, AX (0 to 3), the four ops of `Op`, and programs of exactly six
## instructions. A program's number is its ops as base-4 digits, the first
## op least significant, so the 4,096 programs are the numbers 0 to 4,095.
## A language is the set of inputs a program accepts, ascending, and its
## code is the sum of 2^input over its members, so the 16 languages are the
## codes 0 to 15.
import Op

Machine :: [].{

	## Where a run stands. `Running` after the last op is Python's `None`:
	## the program ran off its end, an implicit reject.
	State : { ax : U64, ending : [Running, Accepted, Rejected] }

	Program : List(Op)

	program_length : U64
	program_length = 6

	program_count : U64
	program_count = 4096

	input_count : U64
	input_count = 4

	language_count : U64
	language_count = 16

	start : U64 -> Machine.State
	start = |input| { ax: input, ending: Running }

	## One instruction. A machine that has accepted or rejected stays as it
	## is, as Python's `continue` skips the rest of the program.
	step : Machine.State, Op -> Machine.State
	step = |machine, op|
		match machine.ending {
			Accepted | Rejected => machine
			Running =>
				match op {
					Nada => machine
					Zero => if (machine.ax == 0) { ..machine, ending: Accepted } else machine
					Decr => if (machine.ax == 0) { ..machine, ending: Rejected } else { ..machine, ax: machine.ax - 1 }
					Mod2 => { ..machine, ax: machine.ax % 2 }
				}
		}

	## Python's `run_progam` (its typo kept out of the name), with AX at the
	## end: where it halted, or after the last op.
	run : Machine.Program, U64 -> Machine.State
	run = |program, input| program.fold(Machine.start(input), Machine.step)

	## The ops as base-4 digits, the first least significant.
	assemble : List(Op) -> U64
	assemble = |ops| ops.fold_rev(0, |op, sum| sum * 4 + op.code())

	## Always six ops; the missing high digits are `nada`.
	disassemble : U64 -> Machine.Program
	disassemble = |number| {
		var $rest = number
		var $ops = List.with_capacity(Machine.program_length)
		for _ in 0..<Machine.program_length {
			$ops = $ops.append(Op.from_code($rest % 4))
			$rest = $rest // 4
		}
		$ops
	}

	language_code : List(U64) -> U64
	language_code = |members| members.fold(0, |code, input| code + U64.shl_wrap(1, input.to_u8_wrap()))

	language : U64 -> List(U64)
	language = |code| List.from_iter(U64.until(0, 32)).keep_if(|input| code.bitwise_and(U64.shl_wrap(1, input.to_u8_wrap())) != 0)

	## The inputs the program accepts, ascending.
	accepted_language : Machine.Program -> List(U64)
	accepted_language = |program| List.from_iter(U64.until(0, Machine.input_count)).keep_if(|input| Machine.run(program, input).ending == Accepted)

	## The inputs a language leaves out.
	complement : List(U64) -> List(U64)
	complement = |members| List.from_iter(U64.until(0, Machine.input_count)).drop_if(|input| members.contains(input))
}

# Python's test(): its six asserts.
expect Machine.assemble([Op.Zero, Op.Decr]) == 9
expect Machine.disassemble(9) == [Op.Zero, Op.Decr, Op.Nada, Op.Nada, Op.Nada, Op.Nada]
expect Machine.language_code([]) == 0
expect Machine.language(0) == []
expect Machine.language_code([1, 3]) == 10
expect Machine.language(10) == [1, 3]

# Each op's rule, on an input where it acts and one where it doesn't.
expect Machine.run(Machine.disassemble(Machine.assemble([Op.Zero])), 0).ending == Accepted
expect Machine.run(Machine.disassemble(Machine.assemble([Op.Zero])), 1) == { ax: 1, ending: Running }
expect Machine.run(Machine.disassemble(Machine.assemble([Op.Decr])), 0) == { ax: 0, ending: Rejected }
expect Machine.run(Machine.disassemble(Machine.assemble([Op.Decr, Op.Decr])), 3) == { ax: 1, ending: Running }
expect Machine.run(Machine.disassemble(Machine.assemble([Op.Mod2])), 3) == { ax: 1, ending: Running }
expect Machine.run(Machine.disassemble(Machine.assemble([Op.Mod2])), 2) == { ax: 0, ending: Running }
# An op after a halt changes nothing: AX keeps its value at the accept.
expect Machine.run(Machine.disassemble(Machine.assemble([Op.Decr, Op.Zero, Op.Decr])), 1) == { ax: 0, ending: Accepted }
expect Machine.complement([1, 3]) == [0, 2]
