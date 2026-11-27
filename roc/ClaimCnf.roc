# A claim about the decider as a CNF, in DIMACS. The claim is "program P
# decides the formal language W": the decider's equations (`circuit_main equations`)
# encoded by Cnf.roc, with unit clauses fixing the 16 inputs to P's and W's
# bits and one asking `decides` = 1. sat/claims.shale hands it to rocsat.
#
#   claim-cnf <program, 0 to 4095> <formal language, 0 to 15> <equation line>...
#
# The equations come one line an argument, as the echo platform reads no
# file. The formal language is its code: input n is in it when bit n is 1, so 13 is
# {0, 2, 3}. The program's number is the book's, line 1 its highest base-4
# digit.
import Cnf
import "golden/equations.txt" as decider : Str

main! = |arguments| {
	program = number_at(arguments, 0, 4096)
	problem = number_at(arguments, 1, 16)
	match (program, problem) {
		(Ok(p), Ok(w)) =>
			match claim(p, w, Str.join_with(arguments.drop_first(2), "\n")) {
				Ok(text) => {
					echo!(text)
					Ok({})
				}
				Err(why) => {
					echo!("claim-cnf: ${why}")
					Err(Exit(1))
				}
			}
		_ => {
			echo!("usage: claim-cnf <program, 0 to 4095> <formal language, 0 to 15> <equation line>...")
			Err(Exit(2))
		}
	}
}

## The argument at `index` as a number below `limit`.
number_at : List(Str), U64, U64 -> Try(U64, [Bad])
number_at = |arguments, index, limit|
	match arguments.get(index) {
		Ok(text) =>
			match U64.from_str(text) {
				Ok(n) if n < limit => Ok(n)
				_ => Err(Bad)
			}
		Err(_) => Err(Bad)
	}

## The claim's DIMACS text, or what stopped it.
claim : U64, U64, Str -> Try(Str, Str)
claim = |program, problem, text| {
	equations = Cnf.equations_of(text) ? |BadEquation(line)| "no equation in `${line}`"
	formula = Cnf.formula_of(equations) ? |e| refusal(e)
	fixed = Cnf.fixed(formula, units(program, problem)) ? |e| refusal(e)
	gates = formula.variables - equations.inputs.len().to_i64_wrap()
	comments = [
		"the decider's ${equations.wires.len().to_str()} equations by Tseitin's encoding: ${equations.inputs.len().to_str()} inputs and ${gates.to_str()} AND and OR gates, constants folded",
		"the claim: program ${program.to_str()} decides ${set_text(problem)}, its 16 inputs fixed and decides = 1",
	]
	Ok(Cnf.dimacs(comments, fixed))
}

refusal : [Undefined(Str), Twice(Str)] -> Str
refusal = |e|
	match e {
		Undefined(name) => "${name} is read before it is defined"
		Twice(name) => "${name} is defined twice"
	}

## The claim's units, in the order of the decider's inputs: line k's two
## bits are the program's k-th base-4 digit, line 1 the highest; want n is bit
## n of the formal language; and decides = 1.
units : U64, U64 -> List({ name : Str, bit : Bool })
units = |program, problem| {
	wants = List.from_iter(U64.until(0, 4)).map(|n| { name: "want${n.to_str()}", bit: bit_at(problem, n) })
	lines = List.from_iter(U64.until(0, 6)).fold(
		[],
		|all, k| {
			digit = (program // power(4, 5 - k)) % 4
			line = (k + 1).to_str()
			all.concat([{ name: "op${line}_hb", bit: digit // 2 == 1 }, { name: "op${line}_lb", bit: digit % 2 == 1 }])
		},
	)
	lines.concat(wants).append({ name: "decides", bit: True })
}

bit_at : U64, U64 -> Bool
bit_at = |n, index| (n // power(2, index)) % 2 == 1

power : U64, U64 -> U64
power = |base, exponent| List.repeat(base, exponent).fold(1, |product, b| product * b)

## A formal language's set of inputs, `{0, 2, 3}`.
set_text : U64 -> Str
set_text = |problem| {
	members = List.from_iter(U64.until(0, 4)).keep_if(|n| bit_at(problem, n)).map(|n| n.to_str())
	"{${Str.join_with(members, ", ")}}"
}

# Program 1689 is zero, decr, decr, zero, decr, zero: 01 10 10 01 10 01.
expect units(1689, 13).map(|u| u.bit) == [False, True, True, False, True, False, False, True, True, False, False, True, True, False, True, True, True]
expect set_text(13) == "{0, 2, 3}" and set_text(0) == "{}"

# A claim about a one-gate decider, whole.
expect claim(5, 1, "inputs: op1_hb op1_lb op2_hb op2_lb op3_hb op3_lb op4_hb op4_lb op5_hb op5_lb op6_hb op6_lb want0 want1 want2 want3\ndecides = want0 AND op6_lb\n") == Ok("c the decider's 1 equations by Tseitin's encoding: 16 inputs and 1 AND and OR gates, constants folded\nc the claim: program 5 decides {0}, its 16 inputs fixed and decides = 1\nc op1_hb = 1\nc op1_lb = 2\nc op2_hb = 3\nc op2_lb = 4\nc op3_hb = 5\nc op3_lb = 6\nc op4_hb = 7\nc op4_lb = 8\nc op5_hb = 9\nc op5_lb = 10\nc op6_hb = 11\nc op6_lb = 12\nc want0 = 13\nc want1 = 14\nc want2 = 15\nc want3 = 16\nc decides = 17\np cnf 17 20\n-17 13 0\n-17 12 0\n17 -13 -12 0\n-1 0\n-2 0\n-3 0\n-4 0\n-5 0\n-6 0\n-7 0\n-8 0\n-9 0\n10 0\n-11 0\n12 0\n13 0\n-14 0\n-15 0\n-16 0\n17 0\n")

# The whole decider, roc/golden/equations.txt (build.shale holds it to
# `circuit_main equations`): its 1,447 lines are 1,439 gates (659 AND, 388 OR,
# 392 NOT) and 8 copies. Once the constants fold, 461 AND and OR gates are
# left, so the CNF has 16 + 461 = 477 variables and 3 · 461 + 17 = 1,400
# clauses, 17 of them the claim's units.
expect {
	text = claim(1689, 13, decider) ?? ""
	text.starts_with("c the decider's 1447 equations by Tseitin's encoding: 16 inputs and 461 AND and OR gates, constants folded\n") and text.contains("\np cnf 477 1400\n") and text.contains("\nc decides = 477\n")
}
