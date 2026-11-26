# fourteen_cnf.py: the Nibble machine's one function, f : {0,1}^14 -> {0,1}
# (roc/Fourteen.roc), as a CNF formula a SAT tool can read, and the claim
# "f is 1 at this point" as a full assignment a model checker can verify.
#
#   python3 python/fourteen_cnf.py cnf             the formula, DIMACS, on stdout
#   python3 python/fourteen_cnf.py model N P       the assignment for input N, program P
#
# The formula's variables: v1 and v2 are the input's two bits (high, low),
# v3 to v14 are the program's twelve, line 1's two first, so v1 .. v14 read
# as the 14-bit number Fourteen.roc's f takes. The step is python/steps.py's
# four functions, composed six times with every bit unknown; each AND and OR
# gate gets a variable of its own (Tseitin's encoding: three clauses tie it
# to its two inputs), and NOT is a literal's sign. The last clause asks that
# `accepted` after line 6 be 1, so the formula's models are the points where
# f is 1, each with its gates' values.
#
# A model for one point is no search: every variable's value is computed by
# running the gates on that point's 14 bits. A checker that finds every
# clause satisfied has verified the claim that f is 1 there.

import sys

import steps
from steps import FALSE, step, var


def formula():
    state = (var(1), var(2), FALSE, FALSE)
    for k in range(1, 7):
        state = step(var(2 * k + 1), var(2 * k + 2), state)
    return state[3]


# Gives each AND and OR gate under `out` a variable from 15 up, children
# first, and returns (literal of, gates in order).
def numbered(out):
    number = {}
    order = []

    def visit(f):
        if f.op in ("var", "const") or f in number:
            return
        for a in f.args:
            visit(a)
        if f.op in ("and", "or"):
            number[f] = 15 + len(order)
            order.append(f)

    visit(out)

    def lit(f):
        if f.op == "var":
            return f.args[0]
        if f.op == "not":
            return -lit(f.args[0])
        if f.op in ("and", "or"):
            return number[f]
        raise ValueError("a constant reached the formula: " + f.text())

    return lit, order


def cnf_text():
    out = formula()
    lit, gates = numbered(out)
    clauses = []
    for g in gates:
        a, b = lit(g.args[0]), lit(g.args[1])
        v = lit(g)
        if g.op == "and":
            clauses += [[-v, a], [-v, b], [v, -a, -b]]
        else:
            clauses += [[v, -a], [v, -b], [-v, a, b]]
    clauses.append([lit(out)])
    head = [
        "c f : {0,1}^14 -> {0,1}, the Nibble machine as one function",
        "c v1 v2: the input's bits; v3 .. v14: the program's, line 1 first",
        f"c v15 .. v{14 + len(gates)}: AND and OR gates; the last clause: f = 1",
        f"p cnf {14 + len(gates)} {len(clauses)}",
    ]
    return "\n".join(head + [" ".join(str(x) for x in c) + " 0" for c in clauses]) + "\n"


def model_text(n, program):
    out = formula()
    lit, gates = numbered(out)
    env = {1: n >> 1, 2: n & 1}
    for k in range(12):
        env[3 + k] = (program >> (11 - k)) & 1
    memo = {}
    values = [env[i] for i in range(1, 15)] + [g.value(env, memo) for g in gates]
    lits = [str(i + 1) if v else str(-(i + 1)) for i, v in enumerate(values)]
    return "v " + " ".join(lits) + " 0\n"


if __name__ == "__main__":
    if sys.argv[1:] == ["cnf"]:
        sys.stdout.write(cnf_text())
    elif len(sys.argv) == 4 and sys.argv[1] == "model":
        sys.stdout.write(model_text(int(sys.argv[2]), int(sys.argv[3])))
    else:
        sys.exit("usage: fourteen_cnf.py cnf | model N P")
