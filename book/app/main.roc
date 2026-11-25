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
import Polynomials
import SatFiles

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
	Builder.build!(
		{ ..settings, fence_hooks: [Diagrams.hook(drawn), Polynomials.hook($polynomials), SatFiles.hook($sat_files)] },
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
## A `.c` file is quoted as C, which the builder's own list leaves out.
settings : Builder.Settings
settings = {
	..Builder.default_settings,
	manuscript: manuscript_path,
	negatives: Lint({ words: "book/negative_words.txt", table: "book/negatives.txt" }),
	colorer: colored_code,
	include_languages: Includes.default_languages.append({ extension: "c", language: "c" }),
	look: { ..default_look, head_end: Polynomials.style },
}

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
