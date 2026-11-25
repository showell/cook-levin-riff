## This module shows a SAT solver's input or output in full, as a `dimacs`
## fence of the manuscript asks:
##
##     ```dimacs PATH
##     where the text comes from
##     COUNT
##     ```
##
## The text is the whole file at PATH: a formula (`.cnf`), a DRAT proof
## (`.drat`) or a solver's answer (`.model`). The box shows its counts, made
## here from the text it shows: a formula's variables, clauses, literals and
## bytes; a proof's lemmas, deletions and bytes; an answer's verdict, the
## variables it names, how many are true, and its bytes. COUNT is what the
## manuscript states, a formula's clauses, a proof's lemmas or an answer's
## variables named, and a text with any other count stops the build, so the
## page and its prose agree. So does a formula whose `p cnf` line states
## another number of clauses than its text holds.
import bb.BookText
import bb.Fences
import Polynomials

SatFiles := [].{

	## A file a `dimacs` fence names, as the build read it.
	Source : { path : Str, text : Str }

	## What the box says about its text: the count the fence states, and the
	## facts over the box.
	Summary : { count : U64, facts : List(Str) }

	## The paths the manuscript's `dimacs` fences name, each once.
	paths_wanted : Str -> List(Str)
	paths_wanted = |manuscript| {
		var $paths = []
		for line in manuscript.split_on("\n") {
			if line.starts_with(opening) {
				path = line.drop_prefix(opening).trim()
				if !$paths.contains(path) {
					$paths = $paths.append(path)
				}
			}
		}
		$paths
	}

	## The fence hook, given every file the fences name.
	hook : List(SatFiles.Source) -> Fences.Hook
	hook = |sources| {
		language: "dimacs",
		html_of: |fence| html_of(fence, sources),
	}

	## A file's summary, by its kind, or why the build stops.
	summary_of : Str, Str -> Try(SatFiles.Summary, Str)
	summary_of = |path, text| {
		if path.ends_with(".cnf") {
			cnf_summary(path, text)
		} else if path.ends_with(".drat") {
			Ok(proof_summary(text))
		} else if path.ends_with(".model") {
			answer_summary(path, text)
		} else {
			Err("${path} is a dimacs fence's file of none of the kinds .cnf, .drat and .model")
		}
	}
}

opening : Str
opening = "```dimacs "

lines_of : Str -> List(Str)
lines_of = |text| text.trim().split_on("\n")

## The literals of a clause or `v` line: its words but `0`, `v` and `d`.
literal_words : Str -> List(Str)
literal_words = |line| {
	var $kept = []
	for word in line.split_on(" ") {
		if word != "" and word != "0" and word != "v" and word != "d" {
			$kept = $kept.append(word)
		}
	}
	$kept
}

bytes_of : Str -> Str
bytes_of = |text| "${Polynomials.with_commas(text.to_utf8().len())} bytes"

is_clause : Str -> Bool
is_clause = |line| line != "" and !line.starts_with("c") and !line.starts_with("p ")

cnf_summary : Str, Str -> Try(SatFiles.Summary, Str)
cnf_summary = |path, text| {
	lines = lines_of(text)
	clauses = lines.count_if(is_clause)
	literals = lines.fold(0, |n, line| if (is_clause(line)) n + literal_words(line).len() else n)
	match lines.find_first(|line| line.starts_with("p cnf ")) {
		Err(NotFound) => Err("${path} has no `p cnf` line")
		Ok(header) =>
			match header.split_on(" ") {
				["p", "cnf", variables, stated] =>
					if stated == clauses.to_str() {
						Ok({
							count: clauses,
							facts: [
								"${variables} variables",
								"${Polynomials.with_commas(clauses)} clauses",
								"${Polynomials.with_commas(literals)} literals",
								bytes_of(text),
							],
						})
					} else {
						Err("${path}'s `p cnf` line says ${stated} clauses, and its text has ${clauses.to_str()}")
					}
				_ => Err("${path}'s `p cnf` line is not `p cnf VARIABLES CLAUSES`")
			}
	}
}

proof_summary : Str -> SatFiles.Summary
proof_summary = |text| {
	lines = lines_of(text)
	lemmas = lines.count_if(|line| is_clause(line) and !line.starts_with("d "))
	deletions = lines.count_if(|line| line.starts_with("d "))
	{
		count: lemmas,
		facts: [
			"${Polynomials.with_commas(lemmas)} lemmas",
			"${Polynomials.with_commas(deletions)} deletions",
			bytes_of(text),
		],
	}
}

answer_summary : Str, Str -> Try(SatFiles.Summary, Str)
answer_summary = |path, text| {
	lines = lines_of(text)
	values = lines.fold([], |words, line| if (line.starts_with("v ")) words.concat(literal_words(line)) else words)
	trues = values.count_if(|word| !word.starts_with("-"))
	match lines.find_first(|line| line.starts_with("s ")) {
		Err(NotFound) => Err("${path} has no `s` line")
		Ok(verdict) =>
			Ok({
				count: values.len(),
				facts: [
					verdict.drop_prefix("s "),
					"${Polynomials.with_commas(values.len())} variables named",
					"${Polynomials.with_commas(trues)} true",
					bytes_of(text),
				],
			})
	}
}

## The box for one fence, or why the build stops.
html_of : Fences.Fence, List(SatFiles.Source) -> [Html(Str), Refused(Str)]
html_of = |{ words, body }, sources| {
	path = words.trim()
	match (sources.find_first(|source| source.path == path), body.trim().split_on("\n")) {
		(Err(NotFound), _) => Refused("the build read no file ${path}")
		(Ok(source), [origin, stated]) =>
			match SatFiles.summary_of(path, source.text) {
				Err(why) => Refused(why)
				Ok({ count, facts }) =>
					if count.to_str() == stated.trim() {
						Html(box_html({ path, origin, facts, text: source.text }))
					} else {
						Refused("${path} counts ${count.to_str()}, and the fence says ${stated.trim()}")
					}
			}
		_ => Refused("a dimacs fence's body is two lines: where the text comes from, then its count")
	}
}

box_html : { path : Str, origin : Str, facts : List(Str), text : Str } -> Str
box_html = |{ path, origin, facts, text }| {
	safe_path = BookText.escaped_text(path)
	safe_origin = BookText.escaped_text(origin)
	safe_facts = BookText.escaped_text(Str.join_with(facts, ", "))
	safe_text = BookText.escaped_text(text.trim())
	box =
		\\<figure class="scrolling"><figcaption><code>${safe_path}</code>: ${safe_origin}<br>${safe_facts}</figcaption><pre><code class="language-text">${safe_text}</code></pre></figure>
	box
}

expect SatFiles.summary_of("x.cnf", "c hi\np cnf 3 2\n1 -2 0\n3 0\n") == Ok({ count: 2, facts: ["3 variables", "2 clauses", "3 literals", "26 bytes"] })
expect
	(SatFiles.summary_of("p.drat", "1 0\nd 1 2 0\n0\n"), SatFiles.summary_of("a.model", "c x\ns SATISFIABLE\nv 1 -2 0\n"))
		== (Ok({ count: 2, facts: ["2 lemmas", "1 deletions", "14 bytes"] }), Ok({ count: 2, facts: ["SATISFIABLE", "2 variables named", "1 true", "27 bytes"] }))
expect
	[
		({ words: "x.cnf", body: "C\n2\n" }, Html("")),
		({ words: "x.cnf", body: "C\n3\n" }, Refused("x.cnf counts 2, and the fence says 3")),
		({ words: "y.cnf", body: "C\n2\n" }, Refused("y.cnf's `p cnf` line says 5 clauses, and its text has 2")),
		({ words: "z.drat", body: "C\n2\n" }, Refused("the build read no file z.drat")),
	].all(
		|(fence, want)| {
			got = (SatFiles.hook([{ path: "x.cnf", text: "p cnf 3 2\n1 -2 0\n3 0\n" }, { path: "y.cnf", text: "p cnf 3 5\n1 0\n2 0\n" }]).html_of)(fence)
			match (got, want) {
				(Html(html), Html(_)) => html.contains("<code>x.cnf</code>: C<br>3 variables, 2 clauses, 3 literals, 21 bytes")
				_ => got == want
			}
		},
	)
