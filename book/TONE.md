# The Cook-Levin book's tone

The book is a careful technical record that is meant to be enjoyed. A writer
adding to `book.md` keeps to these:

- **Talk to the reader.** The prose may say "we" and "you", show wonder,
  and say why a step matters. A machine with a two-bit register whose
  composed polynomial runs to 510,387 characters is worth a sentence of
  amazement, stated with its number.
- **Every chapter opens with a lead**: one to three sentences that make a
  bold claim about what the chapter gives the reader, after a guidepost in
  bold naming its kind (start here, a review of a well-known idea, cool
  math, new theorems, diagrams, big output to skim, history, the tools) and
  the earlier chapter it needs, or that it stands alone. A lead may promise
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
