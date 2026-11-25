## One step of the machine as four polynomials, Steve's stepper.py built in
## the Boolean ring. AX is 2·hb + lb and the op is 2·op_hb + op_lb, and
## each fact about them is an indicator, a polynomial that is 1 when the
## fact holds and 0 when it doesn't. Logic is then arithmetic: NOT x is
## 1 - x, x AND y is x·y, and x OR y is x + y - x·y.
##
## The stepper's `halted` is Python's, kept as it is: a `zero` that accepts
## doesn't set it, so after an accept a later `decr` at AX = 0 can. The
## verdict is `accepted`, which agrees with the machine on every program.
import BoolPoly
import Machine
import Op

Stepper :: [].{

	## The machine between steps, as polynomials. On 0/1 values each one is
	## a bit of the state.
	State : { hb : BoolPoly, lb : BoolPoly, halted : BoolPoly, accepted : BoolPoly }

	## The state after one step, from the state and the op's two bits.
	## `pass`, `check`, `decr` and `mod2` say what each op does, as
	## indicators of where AX goes and whether it accepts or halts; the
	## new state is what any of them does.
	next_state : { hb : BoolPoly, lb : BoolPoly, halted : BoolPoly, accepted : BoolPoly, op_hb : BoolPoly, op_lb : BoolPoly } -> Stepper.State
	next_state = |{ hb, lb, halted, accepted, op_hb, op_lb }| {
		is_0 = complement(hb) * complement(lb)
		is_1 = complement(hb) * lb
		is_2 = hb * complement(lb)
		is_3 = hb * lb

		runs_pass = either(complement(op_hb) * complement(op_lb), halted)
		runs_check = complement(op_hb) * op_lb * complement(halted)
		runs_decr = op_hb * complement(op_lb) * complement(halted)
		runs_mod2 = op_hb * op_lb * complement(halted)

		none = BoolPoly.zero
		pass = { to_1: is_1 * runs_pass, to_2: is_2 * runs_pass, to_3: is_3 * runs_pass, accepts: none, halts: none }
		check = { to_1: is_1 * runs_check, to_2: is_2 * runs_check, to_3: is_3 * runs_check, accepts: is_0 * runs_check, halts: none }
		decr = { to_1: is_2 * runs_decr, to_2: is_3 * runs_decr, to_3: none, accepts: none, halts: is_0 * runs_decr }
		mod2 = { to_1: either(is_3, is_1) * runs_mod2, to_2: none, to_3: none, accepts: none, halts: none }
		ops = [pass, check, decr, mod2]

		becomes_1 = any_of(ops.map(|op| op.to_1))
		becomes_2 = any_of(ops.map(|op| op.to_2))
		becomes_3 = any_of(ops.map(|op| op.to_3))
		{
			hb: either(becomes_3, becomes_2),
			lb: either(becomes_3, becomes_1),
			halted: either(halted, any_of(ops.map(|op| op.halts))),
			accepted: either(accepted, any_of(ops.map(|op| op.accepts))),
		}
	}

	## `next_state` of six variables, in this order: the step itself.
	step_polynomials : Stepper.State
	step_polynomials = Stepper.next_state({
		hb: BoolPoly.variable(0),
		lb: BoolPoly.variable(1),
		halted: BoolPoly.variable(2),
		accepted: BoolPoly.variable(3),
		op_hb: BoolPoly.variable(4),
		op_lb: BoolPoly.variable(5),
	})

	step_variable_names : List(Str)
	step_variable_names = ["hb", "lb", "halted", "accepted", "op_hb", "op_lb"]

	## Python's `step`: the step polynomials evaluated at one state and op.
	step : Stepper.Bits, Op -> Stepper.Bits
	step = |bits, op| {
		# Variable i is bit i: hb, lb, halted, accepted, op_hb, op_lb.
		ones = bits.ax // 2 + 2 * (bits.ax % 2) + 4 * bits.halted + 8 * bits.accepted + 16 * (op.code() // 2) + 32 * (op.code() % 2)
		polys = Stepper.step_polynomials
		{
			ax: 2 * bit_of(polys.hb.eval(ones)) + bit_of(polys.lb.eval(ones)),
			halted: bit_of(polys.halted.eval(ones)),
			accepted: bit_of(polys.accepted.eval(ones)),
		}
	}

	## The stepper's state as numbers: AX from 0 to 3, and two bits.
	Bits : { ax : U64, halted : U64, accepted : U64 }

	## Six steps from an input, nothing halted or accepted yet.
	run : Machine.Program, U64 -> Stepper.Bits
	run = |program, input| program.fold({ ax: input, halted: 0, accepted: 0 }, Stepper.step)

	## Python's test_with_stepper over the inputs: those the stepper accepts.
	accepted_language : Machine.Program -> List(U64)
	accepted_language = |program| List.from_iter(U64.until(0, Machine.input_count)).keep_if(|input| Stepper.run(program, input).accepted == 1)
}

## NOT, as an indicator: 1 - x.
complement : BoolPoly -> BoolPoly
complement = |x| BoolPoly.one - x

## OR, as an indicator: x + y - x·y.
either : BoolPoly, BoolPoly -> BoolPoly
either = |x, y| x + y - x * y

## OR over a list; OR is associative as polynomials, so the order is free.
any_of : List(BoolPoly) -> BoolPoly
any_of = |polys| polys.fold(BoolPoly.zero, either)

## A step polynomial's value, which is a bit on 0/1 values or a stepper bug.
bit_of : I64 -> U64
bit_of = |value|
	match value {
		0 => 0
		1 => 1
		_ => crash "Stepper.bit_of: a step polynomial gave ${value.to_str()}, not 0 or 1"
	}

## The stepper's rule, said in plain code: what the polynomials must give.
reference_step : Stepper.Bits, Op -> Stepper.Bits
reference_step = |bits, op|
	if bits.halted == 1 {
		bits
	} else {
		match op {
			Nada => bits
			Zero => if (bits.ax == 0) { ..bits, accepted: 1 } else bits
			Decr => if (bits.ax == 0) { ..bits, halted: 1 } else { ..bits, ax: bits.ax - 1 }
			Mod2 => { ..bits, ax: bits.ax % 2 }
		}
	}

# The four step polynomials have the dissection's sizes, every coefficient
# 1 or -1.
expect {
	polys = Stepper.step_polynomials
	all = [polys.hb, polys.lb, polys.halted, polys.accepted]
	all.map(|p| p.term_count()) == [7, 13, 17, 33] and all.map(|p| p.degree()) == [5, 5, 5, 6] and all.all(|p| p.max_abs_coeff() == 1)
}

# halted' = halted OR (AX = 0 AND decr AND NOT halted), expanded.
expect
	Stepper.step_polynomials.halted.to_str(Stepper.step_variable_names)
		== "halted+op_hb+(-1)*hb*op_hb+(-1)*lb*op_hb+(-1)*halted*op_hb+(-1)*op_hb*op_lb+hb*lb*op_hb+hb*halted*op_hb+hb*op_hb*op_lb+lb*halted*op_hb+lb*op_hb*op_lb+halted*op_hb*op_lb+(-1)*hb*lb*halted*op_hb+(-1)*hb*lb*op_hb*op_lb+(-1)*hb*halted*op_hb*op_lb+(-1)*lb*halted*op_hb*op_lb+hb*lb*halted*op_hb*op_lb"

# The polynomials agree with the rule on all 64 states and ops.
expect
	List.from_iter(U64.until(0, 64)).all(
		|ones| {
			bits = { ax: ones % 4, halted: (ones // 4) % 2, accepted: (ones // 8) % 2 }
			op = Op.from_code(ones // 16)
			Stepper.step(bits, op) == reference_step(bits, op)
		},
	)
