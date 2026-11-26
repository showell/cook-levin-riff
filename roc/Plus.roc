## Nibble++, as src/plus.c has it: AX is 4 bits (inputs 0 to 15), a program
## is a straight line of `PlusOp`s of any length, and a run that passes the
## last line rejects. A run is Nibble's `Machine.State`, and `zero`, `decr`
## and `mod2` are Nibble's own ops, stepped by `Machine.step`; `half` sets
## AX to AX / 2, rounding down. This is the plain interpreter, one input at
## a time; Census.roc searches the machine's states its own way, and its
## witnesses are held to this.
import Machine
import Op
import PlusOp

Plus :: [].{

	input_count : U64
	input_count = 16

	language_count : U64
	language_count = 65536

	## One line. A run that has accepted or rejected stays as it is.
	step : Machine.State, PlusOp -> Machine.State
	step = |machine, op|
		match op {
			Half =>
				match machine.ending {
					Running => { ..machine, ax: machine.ax // 2 }
					Accepted | Rejected => machine
				}
			Zero => Machine.step(machine, Op.Zero)
			Decr => Machine.step(machine, Op.Decr)
			Mod2 => Machine.step(machine, Op.Mod2)
		}

	run : List(PlusOp), U64 -> Machine.State
	run = |program, input| program.fold(Machine.start(input), Plus.step)

	## The code of the language the program decides: bit n set when input n
	## is accepted.
	language_code : List(PlusOp) -> U64
	language_code = |program|
		Machine.language_code(List.from_iter(U64.until(0, Plus.input_count)).keep_if(|input| Plus.run(program, input).ending == Accepted))
}

# Each op's rule, on an input where it acts and one where it doesn't.
expect Plus.run([PlusOp.Half], 15) == { ax: 7, ending: Running }
expect Plus.run([PlusOp.Half], 0) == { ax: 0, ending: Running }
expect Plus.run([PlusOp.Zero], 0).ending == Accepted
expect Plus.run([PlusOp.Zero], 9) == { ax: 9, ending: Running }
expect Plus.run([PlusOp.Decr], 0) == { ax: 0, ending: Rejected }
expect Plus.run([PlusOp.Decr], 12) == { ax: 11, ending: Running }
expect Plus.run([PlusOp.Mod2], 13) == { ax: 1, ending: Running }
# A halted run stays halted, `half` included.
expect Plus.run([PlusOp.Zero, PlusOp.Half, PlusOp.Decr], 0) == { ax: 0, ending: Accepted }
# `half, zero` accepts 0 and 1; `mod2, zero` accepts the even inputs.
expect Plus.language_code([PlusOp.Half, PlusOp.Zero]) == 3
expect Plus.language_code([PlusOp.Mod2, PlusOp.Zero]) == 0x5555
expect Plus.language_code([]) == 0
