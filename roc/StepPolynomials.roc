# The four step polynomials, one line each, as src/boolring.c's `bp_str`
# prints them; roc/check.shale holds the output to
# roc/golden/step_polynomials.txt, which the C wrote.
import Stepper

main! = |_arguments| {
	polys = Stepper.step_polynomials
	names = Stepper.step_variable_names
	echo!(
		\\hb' = ${polys.hb.to_str(names)}
		\\lb' = ${polys.lb.to_str(names)}
		\\halted' = ${polys.halted.to_str(names)}
		\\accepted' = ${polys.accepted.to_str(names)}
		\\
	)
	Ok({})
}
