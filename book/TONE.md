# The Cook-Levin book's tone

The book is a careful technical record that is meant to be enjoyed. A writer
adding to `book.md` keeps to these:

- **Talk to the reader.** The prose may say "we" and "you", show wonder,
  and say why a step matters. A machine with a two-bit register whose
  composed polynomial runs to 510,387 characters is worth a sentence of
  amazement, stated with its number.
- **Every chapter opens with a lead**: one to three sentences that make a
  bold claim about what the chapter gives the reader, after a guidepost in
  bold naming its kind, one of six fixed kinds (start here; a review; new
  results; the machine in a new form, named by what is newly seen, as in
  "The machine as gates" or "The machine as a grid"; big output to skim;
  tools and history), which the front page explains once, and the earlier chapter it
  needs, or that it stands alone. A "needs" always points backward. The guidepost starts
  with the chapter's frame: ordinary computation, circuit design,
  mathematics, complexity theory or SAT solving. A chapter that changes
  frame partway through says so in one line in italics. A lead may promise
  more than its chapter delivers yet, but never a claim the chapter
  contradicts. The front page lists every chapter with a one-line version
  of its lead, so the book can be skimmed from there.
- **Big output goes at the end of its chapter**, framed by its scale and
  with a word that skimming it is the point; prose never sits below it.
  Each is labelled proudly, like a theorem, `**Monstrosity N: <title>.**`,
  numbered through the book (the build holds the numbers), with a caption
  of measured facts: bytes, lines, terms or clauses, what made it, and from
  how small a machine. A DRAT proof may live in the appendix, its label in
  the chapter pointing there.
- **Program 1689 is the guide.** A chapter that shows the machine in a new
  form shows program 1689 in it, in a ```text fence titled "1689, here: ...",
  which the build generates (`src/guide_main.c`) or quotes; the Nibble++
  chapters follow is_not_14 the same way (`src/guide_plus_main.c`).
- **A theorem is a proof you can run.** Under each theorem a "Run it" box
  gives its test binary and the line it prints, and names its tests; the
  proof's prose stays free of test names and script paths.
- **For developers.** Before a proof that is hard going for a programmer,
  one sentence of intuition in programmer terms.
- **Speed only when it teaches**: a timing stays when it is surprising in an
  educational way, as the census settling everything in under a second
  against SAT's 17 s. No scorecards.
- **The unhappy path is shown, briefly**: where an oracle or a check
  appears, one false input it refuses, stated plainly.
- **A table over a paragraph** wherever the paragraph lists what a table
  shows.
- **Every number is cited**: a commit's short hash, `README.md`, a golden
  file, or a program run while the book was written, named where the
  number is used. A number nobody re-ran is labelled as the record's.
- **An exact figure beats a vague word.** "About nine times faster", "five
  plants", never "much faster" or "many".
- **Present tense for the design, past tense for the history.** What the
  code does now is present; how it got there is dated and past.
- **Dead ends are reported plainly**: what was tried, what it measured, and
  that it was reverted, with no apology.
- **Hedges are precise.** Say exactly what the record shows and what it
  leaves unstated, in plain words, once.
- **Vivid and warm, never hype.** No selling and no superlative the facts
  don't carry. Wonder comes from the facts and their scale.
- **One metaphor per idea**, used once.

The first draft was written impersonally, with no "I", "we" or "you". That
rule was scaffolding: it let the skeleton be built before the tone had to
be right. With the skeleton in place it is retired, and the chapters'
bodies are brought to life as they are touched.

The negatives lint (`negatives.txt`) holds the prose to saying what happens;
a negative stays only under a `keep` kind there.
