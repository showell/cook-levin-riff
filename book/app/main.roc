## This app builds the Cook-Levin book, A riff on Cook-Levin, on the book
## platform with roc-apps' book builder, from the roc-apps checkout beside
## this one. Its folder is the cook-levin checkout's root, so an include
## quotes the repository's C and Roc by their paths there. Its pre-step,
## book.shale, draws the diagrams with Graphviz into `book/build/prepared/`,
## and this app reads the drawings through the platform; a missing one stops
## the build, named. Each `polynomial` fence shows a polynomial file in full,
## as `Polynomials.roc` says; the pre-step composes the four final ones into
## `book/build/out/`. Each `dimacs` fence shows a SAT formula, proof or
## answer in full, as `SatFiles.roc` says; the pre-step writes the formulas
## to `book/build/sat/` and checks rocsat's committed proofs and answers.
## Each `schematic` fence shows one part of the circuit at gate level, as
## `Schematics.roc` says; the pre-step draws them into `book/build/schematics/`.
## Each `equations` fence shows the circuit's Boolean equations in full, as
## `Equations.roc` says; the pre-step writes them to `book/build/`.
app [main!] {
	bb: "../../../roc-apps/book_builder/builder/main.roc",
	pf: platform "../../../roc-apps/book_builder/platform/main.roc",
	sc: "../../../roc-syntax-color/package/main.roc",
}

import pf.Book
import bb.Builder
import bb.Diagrams
import bb.Fences
import bb.Includes
import bb.Pages
import sc.SyntaxColor
import Equations
import Polynomials
import SatFiles
import Schematics

main! : Str => Try(Str, Str)
main! = |_folder| {
	found = Diagrams.found({
		name: manuscript_path,
		text: read_text!(manuscript_path)?,
	}) ? |problems| Str.join_with(problems.map(|problem| "${problem}\n"), "")
	drawn = Diagrams.drawn_diagrams!(
		{
			diagrams: found.diagrams,
			folder: prepared_folder,
			manuscript: manuscript_path,
		},
		read_text!,
	)?
	var $polynomials = []
	for path in Polynomials.paths_wanted(found.manuscript) {
		$polynomials = $polynomials.append({ path, text: read_text!(path)? })
	}
	var $sat_files = []
	for path in SatFiles.paths_wanted(found.manuscript) {
		$sat_files = $sat_files.append({ path, text: read_text!(path)? })
	}
	var $schematics = []
	for path in Schematics.paths_wanted(found.manuscript) {
		$schematics = $schematics.append({ path, text: read_text!(path)? })
	}
	var $equations = []
	for path in Equations.paths_wanted(found.manuscript) {
		$equations = $equations.append({ path, text: read_text!(path)? })
	}
	Builder.build!(
		{ ..settings, fence_hooks: [Diagrams.hook(drawn), Polynomials.hook($polynomials), SatFiles.hook($sat_files), Schematics.hook($schematics), Equations.hook($equations)] },
		|path|
			if path == manuscript_path {
				Ok(found.manuscript)
			} else {
				Book.read_file!(path)
			},
	)
}

manuscript_path : Str
manuscript_path = "book/book.md"

## Where the pre-step leaves the drawings.
prepared_folder : Str
prepared_folder = "book/build/prepared"

## Everything this book chooses, but its fence hook, which needs the drawings.
## Every path is the cook-levin checkout's, since the book quotes its code.
## A `.c` file is quoted as C and a `.py` file as Python, which the
## builder's own list leaves out.
settings : Builder.Settings
settings = {
	..Builder.default_settings,
	manuscript: manuscript_path,
	negatives: Lint({ words: "book/negative_words.txt", table: "book/negatives.txt" }),
	colorer: colored_code,
	include_languages: Includes.default_languages.append({ extension: "c", language: "c" }).append({ extension: "py", language: "python" }),
	look: { ..default_look, head_end: Polynomials.style, dividers: parts },
	word_limit: 1000,
}

## The book's six parts, each named above its first chapter in the
## contents, and the groups of chapters inside them, so the contents can be
## skimmed. A part's title lives here alone; the manuscript names a part by
## its first chapter. The front page lists every chapter with a one-line description.
parts : List(Pages.Divider)
parts = [
	{ before: "The machine", level: Part, title: "Part I: the toy" },
	{ before: "P and NP", level: Part, title: "Part II: P, NP and Cook-Levin" },
	{ before: "P and NP", level: Category, title: "The classes" },
	{ before: "P, NP and Cook-Levin", level: Category, title: "The theorem" },
	{ before: "The Boolean view", level: Part, title: "Part III: Cook-Levin on the toy" },
	{ before: "The Boolean view", level: Category, title: "The machine as logic" },
	{ before: "Parts for a small computer", level: Category, title: "The machine as hardware" },
	{ before: "The Cook-Levin machine", level: Category, title: "Naming: the tableau and SAT" },
	{ before: "The program left unknown", level: Part, title: "Part IV: expanding, the polynomials" },
	{ before: "Nibble++", level: Part, title: "Part V: Nibble++, a bigger machine" },
	{ before: "The oracles", level: Part, title: "Part VI: the cross-checks, and the history" },
	{ before: "What the toy taught us", level: Category, title: "Closing" },
]

default_look : Pages.Look
default_look = Builder.default_settings.look

## Roc, C and Shale colored by roc-syntax-color, and any other language left
## as escaped text.
colored_code : Fences.Colorer
colored_code = |{ language, code }|
	match syntax_of(language) {
		Ok(syntax) => Colored(SyntaxColor.html(syntax, code))
		Err(Uncolored) => Plain
	}

## The colorer of a fence's language, as an include names it.
syntax_of : Str -> Try(SyntaxColor.Language, [Uncolored])
syntax_of = |language| match language {
	"roc" => Ok(SyntaxColor.roc)
	"c" => Ok(SyntaxColor.c)
	"shale" => Ok(SyntaxColor.shale)
	_ => Err(Uncolored)
}

## A file of the book's folder, or the build's stop naming it.
read_text! : Str => Try(Str, Str)
read_text! = |path| {
	problem_line = |problem| "${path} ${Includes.read_problem_text(problem)}\n"
	Book.read_file!(path).map_err(problem_line)
}

## A sample that each of the three languages reads its own way: `#` opens a
## comment in Roc, `--` in Shale, and `//` in C.
sample : Str
sample = "x # a\n-- b\n// c\n"

expect
	[
		("roc", SyntaxColor.roc),
		("c", SyntaxColor.c),
		("shale", SyntaxColor.shale),
	].all(
		|(language, syntax)| colored_code({
			language,
			code: sample,
		}) == Colored(SyntaxColor.html(syntax, sample)),
	)
expect {
	answers = [
		SyntaxColor.roc,
		SyntaxColor.c,
		SyntaxColor.shale,
	].map(|syntax| SyntaxColor.html(syntax, sample))
	answers.all(|answer| answers.count_if(|other| other == answer) == 1)
}
expect [
	"text",
	"markdown",
	"dot",
	"javascript",
	"",
].all(|language| colored_code({ language, code: "a < b" }) == Plain)
