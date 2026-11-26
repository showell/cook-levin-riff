# steps.py: the machine as four Boolean functions, composed over six steps.
#
# A step reads six bits, the line's opcode (op_hb, op_lb) and the state
# (hb, lb, halted, accepted), and writes the four state bits again, each by a
# Boolean function of its own. With the input and the program fixed, the six
# steps composed just run the machine: every bit is a known 0 or 1. With the
# program's twelve bits left unknown, the same composition is a formula.
#
#   python3 python/steps.py
#
# It checks itself against an ordinary interpreter on every program and input.

NADA, ZERO, DECR, MOD2 = 0, 1, 2, 3
NAMES = ["nada", "zero", "decr", "mod2"]


# The ordinary interpreter: zero accepts at 0, decr rejects at 0, and a run
# that gets past its last line rejects.
def run_program(ax, program):
    for op in program:
        if op == ZERO and ax == 0:
            return True
        if op == DECR:
            if ax == 0:
                return False
            ax -= 1
        if op == MOD2:
            ax %= 2
    return False


# One step, written plainly. The gates know only bits, so the step first
# decodes them: the line's two bits into which of the four opcodes it is, and
# AX's two bits into which of the four values it holds. Then each of the four
# new state bits says what every opcode does to it. `~`, `&` and `|` are the
# complement, AND and OR; they work alike on bits and, below, on formulas.


# The line's two bits, op_hb and op_lb, name its opcode.
def opcodes(op_hb, op_lb):
    is_nada = ~op_hb & ~op_lb  # 00
    is_zero = ~op_hb & op_lb  # 01
    is_decr = op_hb & ~op_lb  # 10
    is_mod2 = op_hb & op_lb  # 11
    return is_nada, is_zero, is_decr, is_mod2


# AX's two bits, hb and lb, name its value; the step needs three of them.
def values(hb, lb):
    ax_is_0 = ~hb & ~lb
    ax_is_2 = hb & ~lb
    ax_is_3 = hb & lb
    return ax_is_0, ax_is_2, ax_is_3


def next_hb(op_hb, op_lb, hb, lb, halted, accepted):
    is_nada, is_zero, is_decr, is_mod2 = opcodes(op_hb, op_lb)
    ax_is_0, ax_is_2, ax_is_3 = values(hb, lb)
    # A halted machine keeps its high bit. A running one: nada and zero keep
    # it, decr sets it only when taking 3 to 2, and mod2 clears it.
    return (halted & hb) | (~halted & ((is_nada & hb) | (is_zero & hb) | (is_decr & ax_is_3)))


def next_lb(op_hb, op_lb, hb, lb, halted, accepted):
    is_nada, is_zero, is_decr, is_mod2 = opcodes(op_hb, op_lb)
    ax_is_0, ax_is_2, ax_is_3 = values(hb, lb)
    # A halted machine keeps its low bit. A running one: nada, zero and mod2
    # keep it, and decr sets it only when taking 2 to 1.
    return (halted & lb) | (
        ~halted & ((is_nada & lb) | (is_zero & lb) | (is_mod2 & lb) | (is_decr & ax_is_2))
    )


def next_halted(op_hb, op_lb, hb, lb, halted, accepted):
    is_nada, is_zero, is_decr, is_mod2 = opcodes(op_hb, op_lb)
    ax_is_0, ax_is_2, ax_is_3 = values(hb, lb)
    # Halted already, or a running machine's decr finds AX at 0.
    return halted | (~halted & (is_decr & ax_is_0))


def next_accepted(op_hb, op_lb, hb, lb, halted, accepted):
    is_nada, is_zero, is_decr, is_mod2 = opcodes(op_hb, op_lb)
    ax_is_0, ax_is_2, ax_is_3 = values(hb, lb)
    # Accepted already, or a running machine's zero finds AX at 0.
    return accepted | (~halted & (is_zero & ax_is_0))


def step(op_hb, op_lb, state):
    hb, lb, halted, accepted = state
    args = (op_hb, op_lb, hb, lb, halted, accepted)
    return (next_hb(*args), next_lb(*args), next_halted(*args), next_accepted(*args))


# The same four, golfed: the opcodes merged where they act alike. `~op_hb` is
# "nada or zero", the two opcodes that leave AX alone, and a halted machine
# acts the same way. check() holds each to its plain twin on all 64 settings.
def golfed_hb(op_hb, op_lb, hb, lb, halted, accepted):
    return (hb & (halted | ~op_hb)) | (hb & lb & op_hb & ~op_lb & ~halted)


def golfed_lb(op_hb, op_lb, hb, lb, halted, accepted):
    return (lb & (halted | ~op_hb | (op_hb & op_lb))) | (hb & ~lb & op_hb & ~op_lb & ~halted)


def golfed_halted(op_hb, op_lb, hb, lb, halted, accepted):
    return halted | (~hb & ~lb & op_hb & ~op_lb)


def golfed_accepted(op_hb, op_lb, hb, lb, halted, accepted):
    return accepted | (~hb & ~lb & ~op_hb & op_lb & ~halted)


PLAIN_AND_GOLFED = [
    (next_hb, golfed_hb),
    (next_lb, golfed_lb),
    (next_halted, golfed_halted),
    (next_accepted, golfed_accepted),
]


# Bits as a tiny class, so that ~ & | read as NOT AND OR on 0/1.
class Bit:
    def __init__(self, v):
        self.v = v

    def __invert__(self):
        return Bit(1 - self.v)

    def __and__(self, other):
        return Bit(self.v & other.v)

    def __or__(self, other):
        return Bit(self.v | other.v)

    def __repr__(self):
        return str(self.v)


# Everything fixed: AX and the program are known, so each step's four bits
# are. With `trace`, it also returns one line a step.
def run_with_bits(ax, program, trace=None):
    state = (Bit(ax >> 1), Bit(ax & 1), Bit(0), Bit(0))
    if trace is not None:
        trace.append(f"{'start':<14}" + row(state))
    for k, op in enumerate(program, 1):
        state = step(Bit(op >> 1), Bit(op & 1), state)
        if trace is not None:
            trace.append(f"step {k}  {NAMES[op]} {op >> 1}{op & 1}" + row(state))
    return state[3].v == 1


# One state as the trace prints it, under "hb lb halted accepted".
def row(state):
    hb, lb, halted, accepted = state
    return f"   {hb}  {lb}     {halted}        {accepted}"


# Formulas: the same functions, given unknowns instead of bits. Constants fold
# away and equal subformulas are shared, so a formula is a graph of gates.
class F:
    table = {}

    def __new__(cls, op, *args):
        key = (op, args)
        if key not in F.table:
            f = object.__new__(cls)
            f.op, f.args = op, args
            F.table[key] = f
        return F.table[key]

    def __invert__(self):
        if self is TRUE:
            return FALSE
        if self is FALSE:
            return TRUE
        if self.op == "not":
            return self.args[0]
        return F("not", self)

    def __and__(self, other):
        if self is FALSE or other is FALSE:
            return FALSE
        if self is TRUE:
            return other
        if other is TRUE or self is other:
            return self
        return F("and", self, other)

    def __or__(self, other):
        if self is TRUE or other is TRUE:
            return TRUE
        if self is FALSE:
            return other
        if other is FALSE or self is other:
            return self
        # Absorption: x OR (NOT x AND y) is x OR y.
        if other.op == "and" and other.args[0] is ~self:
            return self | other.args[1]
        return F("or", self, other)

    def value(self, env, memo):
        if self in memo:
            return memo[self]
        if self.op == "const":
            v = self.args[0]
        elif self.op == "var":
            v = env[self.args[0]]
        elif self.op == "not":
            v = 1 - self.args[0].value(env, memo)
        elif self.op == "and":
            v = self.args[0].value(env, memo) & self.args[1].value(env, memo)
        else:
            v = self.args[0].value(env, memo) | self.args[1].value(env, memo)
        memo[self] = v
        return v

    def gates(self, seen=None):
        seen = set() if seen is None else seen
        if self in seen or self.op in ("const", "var"):
            return 0
        seen.add(self)
        return 1 + sum(a.gates(seen) for a in self.args)

    # The operands of a chain of one operator, flattened: (a OR b) OR c is
    # [a, b, c].
    def chain(self, op):
        if self.op != op:
            return [self]
        return self.args[0].chain(op) + self.args[1].chain(op)

    def text(self):
        if self.op == "const":
            return str(self.args[0])
        if self.op == "var":
            return f"v{self.args[0]}"
        if self.op == "not":
            return "NOT " + self.args[0].text()
        sep = " AND " if self.op == "and" else " OR "
        return "(" + sep.join(f.text() for f in self.chain(self.op)) + ")"

    # The formula as its top OR's disjuncts, one a line.
    def lines(self):
        parts = [f.text() for f in self.chain("or")]
        return ["   " + parts[0]] + ["OR " + p for p in parts[1:]]


TRUE, FALSE = F("const", 1), F("const", 0)


def var(i):
    return F("var", i)


# The program unknown: line k's opcode bits are v(2k-1) and v(2k). AX fixed.
def accepted_formula(ax):
    state = (TRUE if ax >> 1 else FALSE, TRUE if ax & 1 else FALSE, FALSE, FALSE)
    for k in range(1, 7):
        state = step(var(2 * k - 1), var(2 * k), state)
    return state[3]


def program_of(code):
    # Program number `code`, line 1 in its high bits: v1 is its top bit.
    return [(code >> (10 - 2 * k)) & 3 for k in range(6)]


def env_of(program):
    env = {}
    for k, op in enumerate(program, 1):
        env[2 * k - 1], env[2 * k] = op >> 1, op & 1
    return env


def check():
    checks = 0
    programs = [program_of(code) for code in range(4096)]
    formulas = [accepted_formula(ax) for ax in range(4)]
    for program in programs:
        env = env_of(program)
        for ax in range(4):
            expected = run_program(ax, program)
            assert run_with_bits(ax, program) == expected
            assert (formulas[ax].value(env, {}) == 1) == expected
            checks += 2
    for setting in range(64):
        bits = [Bit((setting >> i) & 1) for i in range(6)]
        for plain, golfed in PLAIN_AND_GOLFED:
            assert plain(*bits).v == golfed(*bits).v
            checks += 1
    return checks, formulas


# The blocks the book shows, a blank line between them: the run of one
# program at AX = 2, bit by bit; the size of accepted with the program
# unknown, per input; and that formula for AX = 0, written out.
def book_blocks(formulas):
    example = [DECR, MOD2, ZERO, DECR, ZERO, NADA]
    trace = [" " * 14 + "  hb lb halted accepted"]
    assert run_with_bits(2, example, trace)
    blocks = ["```text\n" + "\n".join(trace) + "\n```"]
    rows = ["| AX | gates in accepted |", "|---|---|"]
    rows += [f"| {ax} | {formulas[ax].gates()} |" for ax in range(4)]
    blocks.append("\n".join(rows))
    blocks.append("```text\n" + "\n".join(formulas[0].lines()) + "\n```")
    return blocks


if __name__ == "__main__":
    import sys

    checks, formulas = check()
    if sys.argv[1:] == ["book"]:
        print("\n\n".join(book_blocks(formulas)))
    else:
        print(f"python/steps.py: {checks} checks passed")
