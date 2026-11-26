## This module shows a schematic, one part of the book's circuit drawn at
## gate level, as a `schematic` fence of the manuscript asks:
##
##     ```schematic NAME
##     the caption
##     ```
##
## The drawing is `book/build/schematics/NAME.svg`, which the book's
## pre-step writes with src/circuit_main.c from the circuit's netlist, so a
## drawing is always the netlist's. It goes into the page as it is, in a
## figure over its caption, the way Diagrams places a Graphviz drawing; a
## drawing holding a comment's opening is refused, as Diagrams refuses one,
## since it could close the page early.
import bb.BookText
import bb.Fences

Schematics := [].{

	## A drawing a `schematic` fence names, as the build read it.
	Source : { path : Str, text : Str }

	## The drawings the manuscript's `schematic` fences name, each once.
	paths_wanted : Str -> List(Str)
	paths_wanted = |manuscript| {
		var $paths = []
		for line in manuscript.split_on("\n") {
			if line.starts_with(opening) {
				path = Schematics.path_of(line.drop_prefix(opening).trim())
				if !$paths.contains(path) {
					$paths = $paths.append(path)
				}
			}
		}
		$paths
	}

	## Where the pre-step leaves the drawing named `name`.
	path_of : Str -> Str
	path_of = |name| "book/build/schematics/${name}.svg"

	## The fence hook, given every drawing the fences name.
	hook : List(Schematics.Source) -> Fences.Hook
	hook = |sources| {
		language: "schematic",
		html_of: |fence| html_of(fence, sources),
	}
}

opening : Str
opening = "```schematic "

## The figure for one fence, or why the build stops.
html_of : Fences.Fence, List(Schematics.Source) -> [Html(Str), Refused(Str)]
html_of = |{ words, body }, sources| {
	path = Schematics.path_of(words.trim())
	match sources.find_first(|source| source.path == path) {
		Err(NotFound) => Refused("the build read no drawing ${path}")
		Ok(source) =>
			if source.text.contains("<!--") {
				Refused("the drawing ${path} holds an HTML comment")
			} else {
				match BookText.inline_html(body.trim()) {
					Err(why) => Refused(why)
					Ok(caption) => Html("<figure class=\"diagram\">${source.text.trim()}<figcaption>${caption}</figcaption></figure>")
				}
			}
	}
}

expect Schematics.paths_wanted("a\n```schematic decr\nx\n```\n```schematic mux2\n```\n```schematic decr\n```\n") == ["book/build/schematics/decr.svg", "book/build/schematics/mux2.svg"]
expect
	[
		({ words: "decr", body: "The decrementer\n" }, Html("<figure class=\"diagram\"><svg/><figcaption>The decrementer</figcaption></figure>")),
		({ words: "mod2", body: "x\n" }, Refused("the build read no drawing book/build/schematics/mod2.svg")),
		({ words: "nada", body: "x\n" }, Refused("the drawing book/build/schematics/nada.svg holds an HTML comment")),
	].all(
		|(fence, answer)| (Schematics.hook([{ path: "book/build/schematics/decr.svg", text: "<svg/>\n" }, { path: "book/build/schematics/nada.svg", text: "<svg><!-- x --></svg>" }]).html_of)(fence) == answer,
	)
