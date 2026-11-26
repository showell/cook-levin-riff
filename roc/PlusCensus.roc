# The Nibble++ census report (Census.roc), as src/census_main.c prints it,
# for each opcode set named on the command line:
#
#   plus-census zero,decr zero,decr,mod2 zero,decr,half zero,decr,mod2,half
#
# roc/check.shale holds the output of those four to tests/golden/census.txt
# byte for byte. Before a set's report is printed, every one of its 65,536
# witnesses runs on the plain interpreter (Plus.roc) and must decide its
# language at its census length with the set's ops alone; one that doesn't
# stops the program, named.
#
# The sets are arguments so that the census runs when the program does:
# with constant sets, `roc check` would run the searches at compile time.
import Census
import Plus
import PlusOp

main! = |arguments|
	if arguments.is_empty() {
		usage!({})
	} else {
		match arguments.map(ops_of_name).map_try(|ops| ops) {
			Ok(sets) => {
				echo!(Str.join_with(sets.map(checked_report), ""))
				Ok({})
			}
			Err(NoSuchOp(name)) => {
				echo!("plus-census: no op is named ${name}\n")
				usage!({})
			}
		}
	}

usage! : {} => Try({}, [Exit(I8)])
usage! = |{}| {
	echo!("usage: plus-census <ops, as zero,decr,mod2,half>...\n")
	Err(Exit(2))
}

## The ops of a set's name, `zero,decr,mod2`.
ops_of_name : Str -> Try(List(PlusOp), [NoSuchOp(Str)])
ops_of_name = |text| {
	named = text.split_on(",").map_try(PlusOp.from_name)?
	Ok(PlusOp.all.keep_if(|op| named.contains(op)))
}

## A set's report, once its witnesses are held to the interpreter.
checked_report : List(PlusOp) -> Str
checked_report = |ops| {
	found = Census.search(ops)
	match List.from_iter(U64.until(0, Plus.language_count)).find_first(|language| !is_decided_by_witness(found, language)) {
		Ok(language) => crash "plus-census: ops ${Census.ops_name(ops)}: the witness of language ${language.to_str()} does not decide it"
		Err(NotFound) => Census.report(found)
	}
}

## Whether the language's witness is as long as the census says, uses the
## set's ops alone, and decides exactly the language on the interpreter.
is_decided_by_witness : Census.Found, U64 -> Bool
is_decided_by_witness = |found, language| {
	program = Census.witness(found, language)
	at = found.shortest.get(language) ?? crash "plus-census: a language code is below 65,536"
	program.len() == at.length and program.all(|op| found.ops.contains(op)) and Plus.language_code(program) == language
}
