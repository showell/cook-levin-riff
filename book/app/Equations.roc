## This module shows the circuit's Boolean equations in full, as an
## `equations` fence of the manuscript asks:
##
##     ```equations PATH
##     where the text comes from
##     GATES
##     ```
##
## The text is the file at PATH, the whole of it: `circuit_main equations`'s
## output, which the book's pre-step writes, a line `inputs: ...`, then one
## line a wire, `name = a AND b`, `name = a OR b`, `name = NOT a` or a
## named copy `name = a`. The box shows its counts, counted here from the
## text it shows: its gates, its NOT, AND and OR gates, and its characters.
## GATES is the gate count the manuscript states, and a text with any other
## count stops the build, so the page and its prose agree. The box is
## Polynomials' scrolling box, styled by Polynomials.style.
import bb.BookText
import bb.Fences
import Polynomials

Equations := [].{

	## A file an `equations` fence names, as the build read it.
	Source : { path : Str, text : Str }

	## What the box says about its text.
	Counts : {
		nots : U64,
		ands : U64,
		ors : U64,
		characters : U64,
	}

	## The paths the manuscript's `equations` fences name, each once.
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
	hook : List(Equations.Source) -> Fences.Hook
	hook = |sources| {
		language: "equations",
		html_of: |fence| html_of(fence, sources),
	}

	## A text's gates by kind, and its characters.
	counts_of : Str -> Equations.Counts
	counts_of = |text| {
		var $nots = 0
		var $ands = 0
		var $ors = 0
		for line in text.split_on("\n") {
			if line.contains(" = NOT ") {
				$nots = $nots + 1
			} else if line.contains(" AND ") {
				$ands = $ands + 1
			} else if line.contains(" OR ") {
				$ors = $ors + 1
			}
		}
		{ nots: $nots, ands: $ands, ors: $ors, characters: text.to_utf8().len() }
	}
}

opening : Str
opening = "```equations "

## The box for one fence, or why the build stops.
html_of : Fences.Fence, List(Equations.Source) -> [Html(Str), Refused(Str)]
html_of = |{ words, body }, sources| {
	path = words.trim()
	match (sources.find_first(|source| source.path == path), body.trim().split_on("\n")) {
		(Err(NotFound), _) => Refused("the build read no file ${path}")
		(Ok(source), [origin, stated]) => {
			text = source.text.trim()
			counts = Equations.counts_of(text)
			gates = counts.nots + counts.ands + counts.ors
			if gates.to_str() == stated.trim() {
				Html(box_html({ path, origin, counts, text }))
			} else {
				Refused("${path} has ${gates.to_str()} gates, and the fence says ${stated.trim()}")
			}
		}
		_ => Refused("an equations fence's body is two lines: where the text comes from, then its number of gates")
	}
}

box_html : {
	path : Str,
	origin : Str,
	counts : Equations.Counts,
	text : Str,
} -> Str
box_html = |{ path, origin, counts, text }| {
	gates = counts.nots + counts.ands + counts.ors
	facts = Str.join_with(
		[
			"${Polynomials.with_commas(gates)} gates",
			"${Polynomials.with_commas(counts.nots)} NOT",
			"${Polynomials.with_commas(counts.ands)} AND",
			"${Polynomials.with_commas(counts.ors)} OR",
			"${Polynomials.with_commas(counts.characters)} characters",
		],
		", ",
	)
	safe_path = BookText.escaped_text(path)
	safe_origin = BookText.escaped_text(origin)
	safe_text = BookText.escaped_text(text)
	box =
		\\<figure class="scrolling"><figcaption><code>${safe_path}</code>: ${safe_origin}<br>${facts}</figcaption><pre><code class="language-text">${safe_text}</code></pre></figure>
	box
}

expect Equations.counts_of("inputs: a b\nc = a AND b\nd = NOT c\ne = c OR d\nf = e\n") == { nots: 1, ands: 1, ors: 1, characters: 51 }
expect
	[
		({ words: "e.txt", body: "C\n3\n" }, Bool.True),
		({ words: "e.txt", body: "C\n4\n" }, Bool.False),
		({ words: "f.txt", body: "C\n3\n" }, Bool.False),
	].all(
		|(fence, shown)|
			match (Equations.hook([{ path: "e.txt", text: "inputs: a b\nc = a AND b\nd = NOT c\ne = c OR d\n" }]).html_of)(fence) {
				Html(html) => shown and html.contains("3 gates, 1 NOT, 1 AND, 1 OR")
				Refused(_) => !shown
			},
	)
