## This module shows a polynomial in full, as a `polynomial` fence of the
## manuscript asks:
##
##     ```polynomial PATH [NAME]
##     where the text comes from
##     TERMS
##     ```
##
## The text is the file at PATH, the whole of it, or with a NAME the line of
## it that starts `NAME = `, that prefix taken off. The box shows its
## statistics, counted here from the text it shows: its terms, its degree,
## its largest coefficient and its characters. TERMS is the term count the
## manuscript states, and a text with any other count stops the build, so
## the page and its prose agree.
##
## A polynomial is written as the repository's printers write one, poly.py's
## `str` and the C's `bp_str`: terms joined by `+`, each a coefficient (`3`,
## `(-3)`, or none for 1) and its factors joined by `*`, a power as `(x**5)`.
import bb.BookText
import bb.Fences

Polynomials := [].{

	## A file a `polynomial` fence names, as the build read it.
	Source : { path : Str, text : Str }

	## What the box says about its text.
	Stats : {
		terms : U64,
		degree : U64,
		largest : U64,
		characters : U64,
	}

	## The paths the manuscript's `polynomial` fences name, each once.
	paths_wanted : Str -> List(Str)
	paths_wanted = |manuscript| {
		var $paths = []
		for line in manuscript.split_on("\n") {
			if line.starts_with(opening) {
				path =
					match line.drop_prefix(opening).split_first(" ") {
						Ok({ before, .. }) => before
						Err(NotFound) => line.drop_prefix(opening)
					}
				if !$paths.contains(path) {
					$paths = $paths.append(path)
				}
			}
		}
		$paths
	}

	## The fence hook, given every file the fences name.
	hook : List(Polynomials.Source) -> Fences.Hook
	hook = |sources| {
		language: "polynomial",
		html_of: |fence| html_of(fence, sources),
	}

	## A polynomial's statistics, read from its text in one pass.
	stats_of : Str -> Polynomials.Stats
	stats_of = |text| {
		bytes = text.to_utf8()
		var $terms = 0
		var $degree = 0
		var $largest = 0
		var $term_degree = 0
		var $term_coefficient = 1
		var $at = 0
		while $at < bytes.len() {
			byte = byte_at(bytes, $at)
			if byte == '+' {
				$terms = $terms + 1
				$degree = larger($degree, $term_degree)
				$largest = larger($largest, $term_coefficient)
				$term_degree = 0
				$term_coefficient = 1
				$at = $at + 1
			} else if is_letter(byte) {
				after_name = run_end(bytes, $at, is_name_byte)
				if starts_power(bytes, after_name) {
					after_power = run_end(bytes, after_name + 2, is_digit)
					$term_degree = $term_degree + number_of(bytes, { start: after_name + 2, end: after_power })
					$at = after_power
				} else {
					$term_degree = $term_degree + 1
					$at = after_name
				}
			} else if is_digit(byte) {
				after_number = run_end(bytes, $at, is_digit)
				$term_coefficient = number_of(bytes, { start: $at, end: after_number })
				$at = after_number
			} else {
				$at = $at + 1
			}
		}
		if bytes.is_empty() {
			{ terms: 0, degree: 0, largest: 0, characters: 0 }
		} else {
			{
				terms: $terms + 1,
				degree: larger($degree, $term_degree),
				largest: larger($largest, $term_coefficient),
				characters: bytes.len(),
			}
		}
	}

	## A count with a comma before each group of three digits, as the
	## book's prose writes one.
	with_commas : U64 -> Str
	with_commas = |n| {
		digits = n.to_str().to_utf8()
		var $out = []
		var $at = 0
		for digit in digits {
			left = digits.len() - $at
			if $at > 0 and left % 3 == 0 {
				$out = $out.append(',')
			}
			$out = $out.append(digit)
			$at = $at + 1
		}
		Str.from_utf8($out) ?? crash "Polynomials.with_commas: digits and commas are UTF-8"
	}

	## The stylesheet's rules for a polynomial's box, for the end of each
	## page's head: a fixed height that scrolls down, and the one long line
	## wrapped at any character, so the page never scrolls sideways.
	style : Str
	style =
		\\<style>
		\\figure.polynomial { margin: 1rem 0; background: var(--shade); border-radius: 4px; }
		\\figure.polynomial figcaption { padding: 0.5rem 1rem 0; color: var(--quiet); font: 0.72rem/1.5 var(--mono); }
		\\figure.polynomial pre { margin: 0; padding: 0.3rem 1rem 0.75rem; background: none; max-height: 16rem; overflow-x: hidden; overflow-y: auto; white-space: pre-wrap; overflow-wrap: anywhere; word-break: break-all; font-size: 0.72rem; }
		\\@media (max-width: 700px) { figure.polynomial { margin-inline: -0.5rem; } figure.polynomial figcaption, figure.polynomial pre { padding-inline: 0.6rem; } }
		\\</style>
}

opening : Str
opening = "```polynomial "

## The box for one fence, or why the build stops.
html_of : Fences.Fence, List(Polynomials.Source) -> [Html(Str), Refused(Str)]
html_of = |{ words, body }, sources| {
	{ path, name } =
		match words.split_first(" ") {
			Ok({ before, after }) => { path: before, name: Named(after) }
			Err(NotFound) => { path: words, name: Whole }
		}
	match (sources.find_first(|source| source.path == path), body.trim().split_on("\n")) {
		(Err(NotFound), _) => Refused("the build read no file ${path}")
		(Ok(source), [origin, stated]) =>
			match polynomial_text(source.text, name) {
				Err(Missing(wanted)) => Refused("${path} has no line starting `${wanted} = `")
				Ok(text) => {
					stats = Polynomials.stats_of(text)
					if stats.terms.to_str() == stated.trim() {
						Html(box_html({ title: title_of(path, name), origin, stats, text }))
					} else {
						Refused("${path} has ${stats.terms.to_str()} terms, and the fence says ${stated.trim()}")
					}
				}
			}
		_ => Refused("a polynomial fence's body is two lines: where the text comes from, then its number of terms")
	}
}

## The text a fence shows: the whole file, or one named line of it.
polynomial_text : Str, [Whole, Named(Str)] -> Try(Str, [Missing(Str)])
polynomial_text = |text, name| match name {
	Whole => Ok(text.trim())
	Named(wanted) => {
		prefix = "${wanted} = "
		match text.split_on("\n").find_first(|line| line.starts_with(prefix)) {
			Ok(line) => Ok(line.drop_prefix(prefix).trim())
			Err(NotFound) => Err(Missing(wanted))
		}
	}
}

title_of : Str, [Whole, Named(Str)] -> Str
title_of = |path, name| match name {
	Whole => path
	Named(wanted) => "${wanted} in ${path}"
}

box_html : {
	title : Str,
	origin : Str,
	stats : Polynomials.Stats,
	text : Str,
} -> Str
box_html = |{ title, origin, stats, text }| {
	facts = Str.join_with(
		[
			"${Polynomials.with_commas(stats.terms)} terms",
			"degree ${stats.degree.to_str()}",
			"max |coeff| ${Polynomials.with_commas(stats.largest)}",
			"${Polynomials.with_commas(stats.characters)} characters",
		],
		", ",
	)
	safe_title = BookText.escaped_text(title)
	safe_origin = BookText.escaped_text(origin)
	safe_text = BookText.escaped_text(text)
	box =
		\\<figure class="polynomial"><figcaption><code>${safe_title}</code>: ${safe_origin}<br>${facts}</figcaption><pre><code class="language-text">${safe_text}</code></pre></figure>
	box
}

larger : U64, U64 -> U64
larger = |a, b| if (a > b) a else b

byte_at : List(U8), U64 -> U8
byte_at = |bytes, at| bytes.get(at) ?? crash "Polynomials.byte_at: the index is below the length"

## The index just past the run of bytes from `start` that `keeps` holds.
run_end : List(U8), U64, (U8 -> Bool) -> U64
run_end = |bytes, start, keeps| {
	var $at = start
	while $at < bytes.len() and keeps(byte_at(bytes, $at)) {
		$at = $at + 1
	}
	$at
}

## Whether `**` starts at `at`.
starts_power : List(U8), U64 -> Bool
starts_power = |bytes, at| bytes.get(at) == Ok('*') and bytes.get(at + 1) == Ok('*')

number_of : List(U8), { start : U64, end : U64 } -> U64
number_of = |bytes, { start, end }|
	bytes.sublist({ start, len: end - start }).fold(0, |n, digit| n * 10 + (digit - '0').to_u64())

is_letter : U8 -> Bool
is_letter = |byte| (byte >= 'a' and byte <= 'z') or (byte >= 'A' and byte <= 'Z')

is_digit : U8 -> Bool
is_digit = |byte| byte >= '0' and byte <= '9'

is_name_byte : U8 -> Bool
is_name_byte = |byte| is_letter(byte) or is_digit(byte) or byte == '_'

expect Polynomials.stats_of("(-3)*(x**5)*y+6*x+1+(y**2)") == { terms: 4, degree: 6, largest: 6, characters: 26 }
expect Polynomials.stats_of("op1_hb+(-63)*h0*op2_lb") == { terms: 2, degree: 2, largest: 63, characters: 22 }
expect Polynomials.stats_of("") == { terms: 0, degree: 0, largest: 0, characters: 0 }
expect [(7, "7"), (999, "999"), (1000, "1,000"), (10324, "10,324"), (510387, "510,387"), (1234567, "1,234,567")].all(|(n, text)| Polynomials.with_commas(n) == text)
expect Polynomials.paths_wanted("a\n```polynomial x.txt\nb\n```\n```polynomial y.txt hb'\n```\n```polynomial x.txt lb'\n```\n") == ["x.txt", "y.txt"]
expect
	match (Polynomials.hook([{ path: "p.txt", text: "hb' = x+y\nlb' = x\n" }]).html_of)({ words: "p.txt hb'", body: "Roc\n2\n" }) {
		Html(html) => html.contains(" in p.txt</code>: Roc<br>") and html.contains("2 terms, degree 1, max |coeff| 1, 3 characters")
		Refused(_) => Bool.False
	}
expect
	[
		({ words: "p.txt hb'", body: "Roc\n3\n" }, "p.txt has 2 terms, and the fence says 3"),
		({ words: "p.txt mb'", body: "Roc\n2\n" }, "p.txt has no line starting `mb' = `"),
		({ words: "q.txt", body: "Roc\n2\n" }, "the build read no file q.txt"),
		({ words: "p.txt", body: "Roc\n" }, "a polynomial fence's body is two lines: where the text comes from, then its number of terms"),
	].all(|(fence, why)| (Polynomials.hook([{ path: "p.txt", text: "hb' = x+y\n" }]).html_of)(fence) == Refused(why))
