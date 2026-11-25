## Python's find_solutions: every one of the 4,096 programs run on every
## input, the programs grouped by the language they accept, and for each of
## the 16 languages its count of programs and its first program, printed as
## virtual_machine.py prints them (tests/golden/find_solutions.txt). As in
## the Python, each example program is also run on the stepper's
## polynomials, and a disagreement with the machine is a crash.
import Machine
import Op
import Stepper

Search :: [].{

	## The whole report. A function of `{}`, not a constant: a constant is
	## worked out by `roc check` itself, which has no step limit.
	report : {} -> Str
	report = |{}| {
		codes = List.from_iter(U64.until(0, Machine.program_count)).map(|number| Machine.language_code(Machine.accepted_language(Machine.disassemble(number))))
		sections = List.from_iter(U64.until(0, Machine.language_count)).map(|code| language_section(codes, code))
		Str.join_with(sections, "")
	}
}

## One language's part of the report; `codes` is each program's language
## code, by program number.
language_section : List(U64), U64 -> Str
language_section = |codes, code| {
	language = Machine.language(code)
	first =
		match codes.find_first_index(|c| c == code) {
			Ok(number) => number
			Err(NotFound) => crash "Search.language_section: every language has a program (the golden output says so)"
		}
	example = Machine.disassemble(first.to_u64())
	if Stepper.accepted_language(example) != language {
		crash "Search.language_section: the stepper and the machine disagree on program ${first.to_str()}"
	} else {
		lines = Str.join_with(example.map(|op| "${op.name()} ${op.comment()}\n"), "")
		heading =
			\\${list_text(language)} is solved by ${codes.count_if(|c| c == code).to_str()} possible program
			\\See an example program below.
			\\   it accepts ${list_text(language)}
			\\   it rejects ${list_text(Machine.complement(language))}
			\\--
			\\
		"${heading}${lines}--\n\n"
	}
}

## A list as Python's `str` writes one: `[1, 3]`.
list_text : List(U64) -> Str
list_text = |items| "[${Str.join_with(items.map(|item| item.to_str()), ", ")}]"
