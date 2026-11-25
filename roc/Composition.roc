# The machine's steps composed (Compose.roc): each step's four term counts,
# the last state's sizes, then, once all six steps are composed, the proof
# against the machine at all 2^14 points. roc/check.shale holds the output
# of `composition 6` to roc/golden/composition.txt, whose counts are
# tests/test_compose.c's.
#
#   composition <steps, 1 to 6>
#
# The step count is an argument so that the composition runs when the
# program does: with a constant count, `roc check` composes all six steps
# at compile time.
import BoolPoly
import Compose
import Machine
import Stepper

main! = |arguments| {
	match arguments.get(0) {
		Ok(text) =>
			match U64.from_str(text) {
				Ok(count) if count >= 1 and count <= Machine.program_length => {
					echo!(report(Compose.states(count)))
					Ok({})
				}
				_ => usage!({})
			}
		Err(_) => usage!({})
	}
}

usage! : {} => Try({}, [Exit(I8)])
usage! = |{}| {
	echo!("usage: composition <steps, 1 to 6>\n")
	Err(Exit(2))
}

report : List(Stepper.State) -> Str
report = |states| {
	rows = states.map_with_index(
		|state, index| {
			counts = [state.hb, state.lb, state.halted, state.accepted].map(|p| p.term_count().to_str())
			"${(index + 1).to_str()}  ${Str.join_with(counts, "  ")}\n"
		},
	)
	last = states.last() ?? crash "Composition.report: the count is at least 1"
	sizes = [("hb", last.hb), ("lb", last.lb), ("halted", last.halted), ("accepted", last.accepted)].map(size_line)
	proof_line =
		if states.len() == Machine.program_length {
			proof = Compose.proof(last)
			"proof: ${proof.checked.to_str()} points, ${proof.differ.to_str()} differ, ${proof.accepted.to_str()} accepted, ${proof.accept_without_halt.to_str()} accepted with no halt\n"
		} else {
			"proof: needs all ${Machine.program_length.to_str()} steps\n"
		}
	"step  hb  lb  halted  accepted\n${Str.join_with(rows, "")}${Str.join_with(sizes, "")}${proof_line}"
}

size_line : (Str, BoolPoly) -> Str
size_line = |(name, poly)| "${name}: ${poly.term_count().to_str()} terms, degree ${poly.degree().to_str()}, max |coeff| ${poly.max_abs_coeff().to_str()}\n"
