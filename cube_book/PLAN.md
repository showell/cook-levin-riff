# The cube book's plan: every chapter's title and intro

This is the plan of record for Rolling a Cube to Cook-Levin, written before the chapters. Part I (the first six) has been written and revised since, on the 4 × 4 board with RollScript, so book.md, not this file, is what Part I says now. The later chapters follow this plan. A chunking chapter (macros against the theory) and a Legends appendix (if the book names Turing or Gödel) are planned in addition.

## Easy to check, hard to find

Some answers take seconds to check and ages to find. This book is about that gap, which sits behind the most famous open question in computer science, P versus NP. We won't settle that question; nobody has. Instead you'll study the gap on a toy small enough to hold in your head: a cube that rolls across a board and lights the tiles it lands on. Small boards are easy on purpose, so you can see everything; the hardness shows up as the board grows. By the end you'll have proved the cube's puzzle is as hard as any problem in NP, and watched the Cook-Levin theorem turn it into one logic formula that a SAT solver can attack.

## A programming language with four words

The cube's programming language has four instructions, north, east, south and west, and one rule: when the painted face lands down, the tile under it flips. You'll learn the whole language here, including what happens when a program rolls off the edge, and you'll read the rules as a short Roc program. It is the same code that runs the playground and checks every number in this book.

## Your first programs

Now you write programs. Light the corner, then a tile beside it, then the centre of a 3 × 3 board alone, each in as few rolls as you can. The playground tells you the par for every pattern you make. You'll find that programs can undo their own work, and that the shortest program for a pattern is rarely the obvious one.

## A board where nothing ever happens

We prove our first theorem: on a 2 × 2 board, no program ever lights a tile. The proof follows the paint around the cube and shows it can never reach the bottom. You'll learn to argue with an invariant, a fact every step preserves, which is the same reasoning you use to prove a loop correct.

## Code golf and lower bounds

Writing a short program shows what's possible. Proving that nothing shorter exists is a different skill, and this chapter teaches it. We prove a theorem: lighting m tiles takes at least 3m − 1 rolls, because the paint needs three rolls to travel from one tile to the next. Then you'll see the bound is the best possible, since for every m some pattern meets it exactly.

## Same state, same future

Even a 3 × 3 board has quadrillions of programs, far too many to try. But two programs that leave the cube in the same state behave the same forever after, and the board has only 27,648 states. You'll learn breadth-first search over states, a form of dynamic programming, and write it yourself in a few dozen lines. It finds the shortest program for every one of the 512 patterns in a blink.

## Trust, but verify five times

Every number in this book is computed by several real programs that share nothing but the rules. They are the Roc rules, a census in C, an independent search in Python, a SAT encoding and the playground's engine. You'll learn how they fit together, how each is tested by planting deliberate mistakes, and how to run all of them yourself, before the book leans on them for harder claims.

## P and NP: is finding ever as easy as checking?

P is the class of problems a computer can solve in time polynomial in the size of the input; for the cube, the input is the board's target pattern, n² bits. NP is the class of problems whose yes answers come with a short certificate that can be checked in polynomial time; for the cube, the certificate is a program, and you check it by rolling. P versus NP asks whether every problem in NP is in P. You'll learn both definitions precisely, and why "easy to check" alone never proves a problem is hard to solve.

## What grows and what doesn't

On an n × n board the cube has 6 · n² · 2^(n²) states, a number exponential in the board's area. The certificates are not: we prove that every pattern on every board can be lit in a number of rolls proportional to n². You'll learn why that puts the cube's puzzle in NP, and why a huge state space alone still says nothing about hardness. Shortest paths live in huge spaces too, and they are easy.

## One problem dressed as another

To show a problem is hard, you show that another problem, already known to be hard, can be dressed up as it. That move is a reduction, and it defines NP-completeness: a problem in NP that every other problem in NP reduces to. You'll learn what a reduction is, why it transfers hardness, and meet the Hamiltonian path problem, which asks for a route through a set of grid cells that visits each one exactly once.

## The cube is NP-complete

We prove a theorem: a pattern meets the lower bound of 3m − 1 rolls exactly when its lit tiles form a Hamiltonian path in the grid that starts beside the cube's corner. Deciding that is NP-complete, and with a careful board layout it follows that the cube's puzzle is NP-complete too. You'll see the reduction built step by step, and you'll learn which parts are proved and which are a sketch.

## Every question becomes a formula: the Cook-Levin theorem

The Cook-Levin theorem says that any instance of any problem in NP can be translated, in polynomial time, into its own Boolean formula, which is satisfiable exactly when the instance's answer is yes. The proof works for any polynomial-time checker at all, and that generality is what makes SAT NP-complete: the first problem known to be. You'll learn what Boolean formulas, satisfiability and CNF are, what the theorem states, and what it promises for one cube level.

## Thinking in gates

This chapter takes a short detour into hardware; in the chapters after it, we turn the gates back into Boolean algebra. You'll learn how one roll of the cube becomes a small circuit of AND, OR and NOT gates, with the cube's position, its paint and the tiles going in and the same things one roll later coming out. Every gate is shown, and the circuit is checked against the Roc rules on every possible state.

## Unrolling time

A program runs one roll after another. Copy the one-roll circuit once per roll, leave the rolls themselves unknown, and you get a circuit that is satisfiable exactly when some program solves the level. You'll learn this move, which illustrates the heart of Cook's proof, and see his grid of states, the tableau, filled in for one program.

## Names keep formulas small

Write that circuit out as a formula naively and it explodes. Counterintuitively, adding variables is the cure. Tseitin's encoding gives every gate a name of its own, so the formula grows no faster than the circuit. We prove a theorem: the named formula is satisfiable exactly when the circuit is. You'll see the difference in size for yourself.

## Asking a solver

A SAT solver takes a formula and either finds a satisfying assignment or reports that there isn't one. You'll learn how to ask one about the cube: can this level be solved in k rolls? A yes at k and a no at k − 1 together prove a level's par, and you'll run that pair of questions yourself.

## How to prove "no"

A yes comes with a program you can roll. A no needs a proof. You'll learn what a solver's proof of impossibility looks like, how a small checker replays it step by step, and why this book never takes a solver's word for anything.

## When the solver gets slow

Some levels are easy for the solver, and some take minutes and produce proofs tens of megabytes long. You'll learn which levels are hard, how the cost grows with the board, and how the solver still proves exact pars on a 5 × 5 board, where searching every state is out of reach.

## When a person beats the solver

Ask the solver whether all sixteen tiles can be lit in 46 rolls, and it runs for fifteen minutes without an answer, yet the lower-bound theorem settles it in three lines. You'll learn what a solver sees and what it misses, and why a short human argument is sometimes worth more than any amount of search.

## Why one colour costs more

Light every tile but one on the 4 × 4 board, and it matters which tile you leave dark. Colour the board like a checkerboard, and one colour costs 44 rolls, exactly the lower bound, while the other costs 45. The census handed us that pattern as a puzzle; the Hamiltonian path theorem explains it, because a path through the remaining tiles must alternate colours. You'll learn how a complete census suggests a conjecture, and how a colouring argument turns it into a proof.

## Open problems

Several questions are still waiting. Is the costliest pattern always every tile, at 3n² − 1 rolls? What are the pars on a 5 × 5 board? Can the reduction be made simpler? You'll learn what we know, what we only suspect, and how to start on any of it with the programs from this book.
