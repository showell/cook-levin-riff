## All six steps of the machine composed in the Boolean ring: the step
## polynomials substituted into themselves, so the machine's state after
## the sixth step is four polynomials in 14 variables, the input's two bits
## (`h0`, `l0`) and each instruction's two (`op1_hb`, `op1_lb`, ...).
## Every (input, program) pair is one point of those 14 variables, a 1 bit
## where the variable is 1, so the four polynomials are the whole machine.
import BoolPoly
import Machine
import Op
import Stepper

Compose :: [].{

	variable_count : U64
	variable_count = 14

	variable_names : List(Str)
	variable_names = ["h0", "l0", "op1_hb", "op1_lb", "op2_hb", "op2_lb", "op3_hb", "op3_lb", "op4_hb", "op4_lb", "op5_hb", "op5_lb", "op6_hb", "op6_lb"]

	## Before the first step: AX is the input, nothing halted or accepted.
	start : Stepper.State
	start = { hb: BoolPoly.variable(0), lb: BoolPoly.variable(1), halted: BoolPoly.zero, accepted: BoolPoly.zero }

	## The state after step `index` (1 to 6), from the state before it: each
	## step polynomial with its six variables replaced by the state's four
	## polynomials and the instruction's two variables.
	after_step : Stepper.State, U64 -> Stepper.State
	after_step = |before, index| {
		replacements = [before.hb, before.lb, before.halted, before.accepted, BoolPoly.variable(2 * index), BoolPoly.variable(2 * index + 1)]
		step = Stepper.step_polynomials
		{
			hb: step.hb.substitute(replacements),
			lb: step.lb.substitute(replacements),
			halted: step.halted.substitute(replacements),
			accepted: step.accepted.substitute(replacements),
		}
	}

	## The states after steps 1 to `count`, in order. A function of the
	## count, not a constant, so `roc check` doesn't compose at compile time.
	states : U64 -> List(Stepper.State)
	states = |count| {
		var $state = Compose.start
		var $out = List.with_capacity(count)
		for index in 1..=count {
			$state = Compose.after_step($state, index)
			$out = $out.append($state)
		}
		$out
	}

	## The point of an input and a program: bit i is variable i's value.
	point : U64, Machine.Program -> U64
	point = |input, program| {
		start_bits = input // 2 + 2 * (input % 2)
		program.fold_with_index(start_bits, |bits, op, at| bits + U64.shl_wrap(op_bits(op), (2 * (at + 1)).to_u8_wrap()))
	}

	input_of : U64 -> U64
	input_of = |ones| 2 * bit_at(ones, 0) + bit_at(ones, 1)

	program_of : U64 -> Machine.Program
	program_of = |ones|
		List.from_iter(U64.until(1, Machine.program_length + 1)).map(
			|index| {
				hb = bit_at(ones, 2 * index)
				lb = bit_at(ones, 2 * index + 1)
				Op.from_code(2 * hb + lb)
			},
		)

	## The four final polynomials against the machine at every point.
	proof : Stepper.State -> Compose.Proof
	proof = |final| {
		count = Compose.variable_count
		hb = final.hb.values(count)
		lb = final.lb.values(count)
		halted = final.halted.values(count)
		accepted = final.accepted.values(count)
		List.from_iter(U64.until(0, U64.shl_wrap(1, count.to_u8_wrap()))).fold(
			{ checked: 0, differ: 0, accepted: 0, accept_without_halt: 0 },
			|so_far, ones| {
				input = Compose.input_of(ones)
				program = Compose.program_of(ones)
				machine = Machine.run(program, input)
				stepper = Stepper.run(program, input)
				got = { hb: at(hb, ones), lb: at(lb, ones), halted: at(halted, ones), accepted: at(accepted, ones) }
				want_accepted = if (machine.ending == Accepted) 1 else 0
				agrees =
					got.accepted == want_accepted
						and got.hb == (machine.ax // 2).to_i64_wrap()
						and got.lb == (machine.ax % 2).to_i64_wrap()
						and got.halted == stepper.halted.to_i64_wrap()
						and got.halted == halts_by_rule(program, input)
						and Compose.point(input, program) == ones
						and (ones % 61 != 0 or final.accepted.eval(ones) == got.accepted)
				halted_by_machine = if (machine.ending == Running) 0 else 1
				{
					checked: so_far.checked + 1,
					differ: so_far.differ + (if (agrees) 0 else 1),
					accepted: so_far.accepted + want_accepted.to_u64_wrap(),
					accept_without_halt: so_far.accept_without_halt + (if (got.halted == halted_by_machine) 0 else 1),
				}
			},
		)
	}

	## What the proof counts: the points checked, those where a polynomial
	## and the machine differ, the runs the machine accepts, and the runs
	## where the stepper's halted is not the machine's (an accept with no
	## `decr` after it, which the stepper doesn't count as a halt).
	Proof : { checked : U64, differ : U64, accepted : U64, accept_without_halt : U64 }
}

bit_at : U64, U64 -> U64
bit_at = |ones, index| (ones // U64.shl_wrap(1, index.to_u8_wrap())) % 2

op_bits : Op -> U64
op_bits = |op| op.code() // 2 + 2 * (op.code() % 2)

at : List(I64), U64 -> I64
at = |table, ones| table.get(ones) ?? crash "Compose.at: every point is in the table"

## The stepper's halted by its rule: rejected, or accepted with a `decr`
## after the accepting `zero` (AX is 0 from then on, so that decr halts).
halts_by_rule : Machine.Program, U64 -> I64
halts_by_rule = |program, input| {
	ending = program.fold(
		{ machine: Machine.start(input), halts: 0 },
		|so_far, op|
			match (so_far.machine.ending, op) {
				(Accepted, Decr) => { ..so_far, halts: 1 }
				_ => { ..so_far, machine: Machine.step(so_far.machine, op) }
			},
	)
	if (ending.machine.ending == Rejected) 1 else ending.halts
}

# Program 9 (zero, decr, nada...) on input 2: h0, op1's low bit (zero is
# code 1) and op2's high bit (decr is code 2).
expect Compose.point(2, Machine.disassemble(9)) == 1 + 8 + 16
expect Compose.input_of(1 + 8 + 16) == 2 and Compose.program_of(1 + 8 + 16) == Machine.disassemble(9)
