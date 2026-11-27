# The cube book's tone

Rolling a Cube to Cook-Levin teaches P versus NP to programmers through one
toy, and it should be a pleasure to read cover to cover. A writer adding to
`cube_book/book.md` keeps to these rules. They replace the old book's
`book/TONE.md` for this book.

## One lesson a chapter

- **At most 500 words of prose a chapter.** The build holds it
  (`word_limit` in `cube_book/app/main.roc`); the book builder counts a
  table's cells as prose, so a big table goes in a ```text fence. A
  chapter that can't fit its lesson is cut first, and split only when the
  lesson itself won't fit.
- **The chapters keep their order.** A chapter is revised where it stands
  and never moved. An idea a later chapter needs is introduced, briefly,
  where it is first used.
- **Say each thing once.** The front page lists the chapters by title
  alone. A chapter's lead says what it is about in one or two sentences,
  and its first section starts where the lead stops. The code's comments
  explain the code, and the prose leaves them to it.
- **Build talk lives in the appendix**: how the numbers are checked, which
  file computes what, and the parts of the Roc that only the build needs.

## The reader and the playground

- **The book is tied to the playground.** Every challenge and example runs
  on its 4 × 4 board, and the prose sends the reader there to try it, not
  to a pencil. The 2 × 2 and 3 × 3 boards appear only where a small board
  proves or shows something the big one can't.
- **The playground's words are the book's.** A roll off the board halts
  the program. A program's result is its lit set. Cells are numbered 0 to
  15 across each row from the north-west corner, with the chessboard names
  a1 to d4 beside them, so a lit set is a 16-bit number.
- **Programs answer questions about numbers.** A lit set is a set of the
  numbers 0 to 15, so a challenge is a predicate (`is_even`,
  `is_small_even`, `is_power_of_2`) and a program answers it for every n at
  once, tile n lit for yes. The book lets the reader discover that RollScript
  programs describe formal languages before it says so.

## Looking for answers

- **Say what is being looked for before looking.** Before a result that a
  procedure computed, one sentence says what it looks for. If that sentence
  can't be written yet, the result comes too early in the book.
- **A procedure gets a coined two-word name, never "search".** The blind
  tryout runs every program of each length in turn; the state flood rolls
  out from the start state one distance at a time. A new procedure gets a
  new name of the same kind, coined where it first appears.
- **Results come after their source, or say where it is.** A chapter that
  uses the state flood before {{chapter Same state, same future}} builds it
  names it and says what it found, in a sentence.

## Claims

- **Every number comes from a program.** `cube_book/book.shale` holds each
  table, trace and fact to the output of the program that printed it, and
  every number in the prose to some program's output (check_numbers). A new
  number needs a program behind it first.
- **Theory is stated exactly, in plain words.** Polynomial time, not
  "fast". NP-complete is "in NP, and as hard as any problem in NP". The
  Cook-Levin theorem says SAT is as hard as every NP problem, and the
  construction behind it turns the cube into a formula. A fixed board is a
  finite problem; hardness is about how the work grows with the board.
- **A theorem is stated where it holds.** Each is hunted for its edge cases
  (the empty program, a halt, a board's edge) and stated so they pass.
- **Exact figures over vague words.** "47 rolls", never "many rolls"; a
  timing only with a measurement behind it, and only when it teaches.

## The prose

- **Whole sentences.** No bold labels, no fragments, no bullet lists where a
  sentence reads better, and a table wherever a paragraph would list what a
  table shows.
- **No guideposts** ("Here is the toy in one paragraph", "This chapter
  teaches"). The content is the guide.
- **No presumed reactions.** Say what is surprising by the facts; never
  tell the reader what they feel.
- **Talk to the reader.** "We" and "you" are welcome, and so is wonder,
  stated with its number.
- **Say what happens.** The negatives lint (`cube_book/negatives.txt`) holds
  the prose to positive statements; a negative stays only where the "no" is
  a theorem's or the state of knowledge.
- **Vivid and warm, never hype.** One metaphor per idea, used once.
