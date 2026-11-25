# What virtual_machine.py prints when it runs: find_solutions' report, byte
# for byte tests/golden/find_solutions.txt (roc/check.shale holds it so).
# Python's test() asserts are Machine.roc's expects.
import Search

main! = |_arguments| {
	echo!(Search.report)
	Ok({})
}
