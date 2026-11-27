app [main!] { pf: platform "https://github.com/roc-lang/basic-cli/releases/download/0.22.0/F1JVZPYfWP71s8vk6tHcV1Qx1Ef6CZkwswGoCn8VHZmL.tar.zst" }

# sat/rocsat_guided.roc: the guided search's loop on rocsat's incremental
# API (its Rocsat.roc), one solver kept across the rounds. It is built
# beside a copy of rocsat's own modules, by rocsat's pinned Roc, by
# sat/plus_guided.shale (out/rocsat_guided/), and is no part of rocsat.
#
#   rocsat-guided <guarded.cnf> <first inputs, as 0,15> <proof.drat>
#
# The CNF is `plus_sat search-guarded W K`'s: 16 runs of the program, each
# run's output held by its selector, and `c guard N S G` lines naming input
# N's selector S and goal literal G. Each round solves under the assumptions
# S for the inputs of the set. A SAT round prints `r SAT <set>` and its
# program bits (`b 1 -2 ... 0`); the lowest input outside the set whose goal
# the model makes false joins it, and when none does, the search ends SAT:
# rocsat's stats, `s SATISFIABLE` and the model's `v` lines. An UNSAT round
# prints `r UNSAT <set>`, the final conflict (`u -s1 -s2 ... 0`, the clause of
# the failed assumptions), rocsat's stats and `s UNSATISFIABLE`, and writes
# the proof: rocsat's DRAT lemmas from every round, then that clause and the
# empty clause, which src/drat.c checks against the CNF with the set's
# selectors as unit clauses. The script, not this program, holds every round
# to the interpreter.

import pf.OsStr
import pf.Stdout
import pf.Stderr
import pf.Path
import Dimacs
import Model
import Rocsat

main! : List(OsStr) => Try({}, _)
main! = |args| {
    match args.drop_first(1).map(OsStr.display) {
        [cnf_file, first, proof_file] => guided!(cnf_file, first, proof_file)
        _ => stop!("usage: rocsat-guided <guarded.cnf> <first inputs, as 0,15> <proof.drat>")
    }
}

stop! : Str => Try({}, _)
stop! = |why| {
    Stderr.line!("c error: ${why}")?
    Err(Exit(1))
}

guided! : Str, Str, Str => Try({}, _)
guided! = |cnf_file, first, proof_file| {
    bytes = Path.utf8(cnf_file).read_bytes!()?
    cnf = match Dimacs.parse(bytes) {
        Ok(c) => c
        Err(e) => return stop!("${cnf_file}: ${Str.inspect(e)}")
    }
    guards = match guards_of(bytes) {
        Ok(g) => g
        Err(why) => return stop!("${cnf_file}: ${why}")
    }
    start = match inputs_of(first) {
        Ok(s) => s
        Err(why) => return stop!(why)
    }
    program_vars = guards.fold(cnf.num_vars, |least, g| least.min(g.selector.abs().to_u64_wrap() - 1))
    var $state = Rocsat.from_cnf(cnf, Bool.True)
    var $inputs = start
    var $done = Bool.False
    while !$done {
        assumptions = $inputs.map(|n| selector_of(guards, n))
        (next, result) = match Rocsat.solve($state, assumptions) {
            Ok(answer) => answer
            Err(err) => return stop!(Str.inspect(err))
        }
        $state = next
        set = Str.join_with($inputs.map(|n| n.to_str()), ",")
        match result {
            Unsat => {
                final = Rocsat.final_conflict($state)
                Stdout.line!("r UNSAT ${set}")?
                Stdout.line!(clause_text("u", final))?
                proof = Rocsat.proof_bytes($state).concat("${clause_text("", final)}\n0\n".to_utf8())
                Path.utf8(proof_file).write_bytes!(proof)?
                Stdout.line!(Rocsat.stats($state))?
                Stdout.line!("s UNSATISFIABLE")?
                $done = Bool.True
            }
            Sat => {
                model = Rocsat.model($state)
                Stdout.line!("r SAT ${set}")?
                Stdout.line!(program_text(model, program_vars))?
                match first_wrong(guards, $inputs, model) {
                    Ok(n) => {
                        $inputs = $inputs.append(n)
                    }
                    Err(NotFound) => {
                        Stdout.line!(Rocsat.stats($state))?
                        Stdout.line!("s SATISFIABLE")?
                        for line in Model.format_v_lines(model) {
                            Stdout.line!(line)?
                        }
                        $done = Bool.True
                    }
                }
            }
        }
    }
    Ok({})
}

## The `c guard N S G` lines, in the order of N, 0 to 15.
guards_of : List(U8) -> Try(List({ input : U64, selector : I32, goal : [Literal(I32), Always, Never] }), Str)
guards_of = |bytes| {
    text = match Str.from_utf8(bytes) {
        Ok(t) => t
        Err(_) => return Err("not UTF-8")
    }
    var $guards = []
    for line in text.split_on("\n") {
        if line.starts_with("c guard ") {
            match line.split_on(" ") {
                ["c", "guard", n, s, g] => {
                    input = U64.from_str(n) ? |_| "a guard's input in `${line}`"
                    selector = I32.from_str(s) ? |_| "a guard's selector in `${line}`"
                    goal = if g == "true" {
                        Always
                    } else if g == "false" {
                        Never
                    } else {
                        Literal(I32.from_str(g) ? |_| "a guard's goal in `${line}`")
                    }
                    $guards = $guards.append({ input, selector, goal })
                }
                _ => return Err("a guard line `${line}`")
            }
        }
    }
    if $guards.len() != 16 or $guards.map(|g| g.input) != List.from_iter(U64.until(0, 16)) {
        Err("${$guards.len().to_str()} guard lines, not one for each input 0 to 15 in order")
    } else {
        Ok($guards)
    }
}

## `0,15` as a list of inputs.
inputs_of : Str -> Try(List(U64), Str)
inputs_of = |text| {
    var $inputs = []
    for part in text.split_on(",") {
        n = U64.from_str(part) ? |_| "an input `${part}` in `${text}`"
        if n > 15 {
            return Err("an input `${part}` past 15")
        }
        $inputs = $inputs.append(n)
    }
    Ok($inputs)
}

selector_of : List({ input : U64, selector : I32, goal : [Literal(I32), Always, Never] }), U64 -> I32
selector_of = |guards, n| (guards.get(n) ?? crash "rocsat-guided: an input below 16").selector

## Whether a model makes a signed literal true.
holds : Model.Assignment, I32 -> Bool
holds = |model, lit| {
    v = model.get(lit.abs().to_u64_wrap() - 1) ?? Model.unassigned
    if lit > 0 { v == Model.true_val } else { v == Model.false_val }
}

## The lowest input outside the set whose goal the model makes false.
first_wrong : List({ input : U64, selector : I32, goal : [Literal(I32), Always, Never] }), List(U64), Model.Assignment -> Try(U64, [NotFound])
first_wrong = |guards, inputs, model| {
    wrong = guards.find_first(
        |g| {
            if inputs.contains(g.input) {
                Bool.False
            } else {
                match g.goal {
                    Always => Bool.False
                    Never => Bool.True
                    Literal(lit) => !holds(model, lit)
                }
            }
        },
    )
    match wrong {
        Ok(g) => Ok(g.input)
        Err(_) => Err(NotFound)
    }
}

## `b 1 -2 ... 0`: the program bits, variables 1 to `n`, as the model has them.
program_text : Model.Assignment, U64 -> Str
program_text = |model, n| {
    var $line = "b"
    var $v = 1
    while $v <= n {
        lit = $v.to_i32_wrap()
        $line = $line.concat(if holds(model, lit) { " ${lit.to_str()}" } else { " -${lit.to_str()}" })
        $v = $v + 1
    }
    $line.concat(" 0")
}

## A clause's text, after `prefix`: `u -17 -18 0`.
clause_text : Str, List(I32) -> Str
clause_text = |prefix, lits| {
    var $line = prefix
    for lit in lits {
        $line = if $line == "" { lit.to_str() } else { $line.concat(" ${lit.to_str()}") }
    }
    if $line == "" { "0" } else { $line.concat(" 0") }
}
