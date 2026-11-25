// Cook-Levin in JavaScript: the machine, the search over all 4,096
// programs, the step polynomials built in the Boolean ring, and all six
// steps composed, with the proof against the machine at every point.
//
// It is written in the subset of JavaScript that roc-js-interpreter's
// bytecode VM (js/) and its x86-64 compiler (x86/) both take, so node, the
// VM and the compiled executable are three oracles for one program. The
// subset has no bitwise operators, so a mask's OR and AND are read from
// tables of two 7-bit halves (14 variables make every mask below 2^14), and
// a bit is a division and a `%`. Every value is a small integer, exact in a
// double. The compiled program has no garbage collector yet, so the big
// arrays (the dense table like terms are summed in, the values of the
// proof) are made once and cleared for reuse.
//
// It prints find_solutions' report (tests/golden/find_solutions.txt), then
// the composition's (roc/golden/composition.txt). STEPS is how many steps
// to compose; the proof needs all six (js/oracle.shale may set it lower).

const STEPS = 6;

// ---------------------------------------------------------------- the machine

const OP_NAMES = ["nada", "zero", "decr", "mod2"];
const OP_COMMENTS = [
  "# do nothing",
  "# accept original input if AX = 0",
  "# reject zero or decrement AX",
  "# subtract 2 from AX if AX >= 2",
];
const NADA = 0;
const ZERO = 1;
const DECR = 2;
const MOD2 = 3;
const RUNNING = 0;
const ACCEPTED = 1;
const REJECTED = 2;
const PROGRAM_LENGTH = 6;
const INPUT_COUNT = 4;
const PROGRAM_COUNT = 4096;
const LANGUAGE_COUNT = 16;

// The machine after one op: `{ ax, ending }`, ending RUNNING until it halts.
function machineStep(m, op) {
  if (m.ending !== RUNNING) return m;
  if (op === ZERO) {
    if (m.ax === 0) return { ax: 0, ending: ACCEPTED };
    return m;
  }
  if (op === DECR) {
    if (m.ax === 0) return { ax: 0, ending: REJECTED };
    return { ax: m.ax - 1, ending: RUNNING };
  }
  if (op === MOD2) return { ax: m.ax % 2, ending: RUNNING };
  return m;
}

function machineRun(program, input) {
  let m = { ax: input, ending: RUNNING };
  for (let i = 0; i < PROGRAM_LENGTH; i++) m = machineStep(m, program[i]);
  return m;
}

function disassemble(n) {
  const program = [];
  let rest = n;
  for (let i = 0; i < PROGRAM_LENGTH; i++) {
    program.push(rest % 4);
    rest = Math.floor(rest / 4);
  }
  return program;
}

// The inputs a language's code holds: bit i of the code is input i.
function language(code) {
  const lang = [];
  let rest = code;
  let i = 0;
  while (rest > 0) {
    if (rest % 2 === 1) lang.push(i);
    rest = Math.floor(rest / 2);
    i++;
  }
  return lang;
}

function acceptedCode(program) {
  let code = 0;
  let place = 1;
  for (let input = 0; input < INPUT_COUNT; input++) {
    if (machineRun(program, input).ending === ACCEPTED) code += place;
    place *= 2;
  }
  return code;
}

function complementOf(lang) {
  const out = [];
  for (let i = 0; i < INPUT_COUNT; i++) {
    if (!lang.includes(i)) out.push(i);
  }
  return out;
}

function langText(lang) {
  return "[" + lang.join(", ") + "]";
}

// ------------------------------------------------------- the Boolean ring
//
// A polynomial is `{ masks, coeffs }`: its terms ascend by mask, each mask
// once, no coefficient 0. A monomial is a set of variables, bit i of its
// mask for variable i, and the product of two is their union.

const VARIABLE_LIMIT = 14;
const SIZE = 16384;
const HALF = 128;

const POW2 = [];
POW2.push(1);
for (let i = 1; i <= VARIABLE_LIMIT; i++) POW2.push(2 * POW2[i - 1]);

// OR7[a * 128 + b] and AND7[a * 128 + b] for 7-bit a and b; POP[m] the bits
// of m.
const OR7 = [];
const AND7 = [];
for (let a = 0; a < HALF; a++) {
  for (let b = 0; b < HALF; b++) {
    let or = 0;
    let and = 0;
    let x = a;
    let y = b;
    for (let i = 0; i < 7; i++) {
      const xb = x % 2;
      const yb = y % 2;
      if (xb === 1 || yb === 1) or += POW2[i];
      if (xb === 1 && yb === 1) and += POW2[i];
      x = Math.floor(x / 2);
      y = Math.floor(y / 2);
    }
    OR7.push(or);
    AND7.push(and);
  }
}
const POP = [0];
for (let m = 1; m < SIZE; m++) POP.push(POP[Math.floor(m / 2)] + (m % 2));

function or14(a, b) {
  return OR7[Math.floor(a / HALF) * HALF + Math.floor(b / HALF)] * HALF + OR7[(a % HALF) * HALF + (b % HALF)];
}

function and14(a, b) {
  return AND7[Math.floor(a / HALF) * HALF + Math.floor(b / HALF)] * HALF + AND7[(a % HALF) * HALF + (b % HALF)];
}

// The dense table like terms are summed in, by mask: made once, every slot
// 0 between uses. QH and QL hold one factor's masks split into halves.
const SUMS = [];
for (let m = 0; m < SIZE; m++) SUMS.push(0);
const QH = [];
const QL = [];

// The table's slots up to `width` as a canonical polynomial, each slot put
// back to 0.
function fromSums(width) {
  const masks = [];
  const coeffs = [];
  for (let m = 0; m <= width; m++) {
    const c = SUMS[m];
    if (c !== 0) {
      if (c > 9007199254740991 || c < -9007199254740991) throw new RangeError("a coefficient past 2^53");
      masks.push(m);
      coeffs.push(c);
      SUMS[m] = 0;
    }
  }
  return { masks, coeffs };
}

function unionOf(p) {
  let u = 0;
  for (const m of p.masks) u = or14(u, m);
  return u;
}

const ZERO_POLY = { masks: [], coeffs: [] };
const ONE_POLY = { masks: [0], coeffs: [1] };

function variable(index) {
  if (index >= VARIABLE_LIMIT) throw new RangeError(`variable ${index} is past the last`);
  return { masks: [POW2[index]], coeffs: [1] };
}

// p + sign * q.
function added(p, q, sign) {
  for (let i = 0; i < p.masks.length; i++) SUMS[p.masks[i]] += p.coeffs[i];
  for (let i = 0; i < q.masks.length; i++) SUMS[q.masks[i]] += sign * q.coeffs[i];
  return fromSums(or14(unionOf(p), unionOf(q)));
}

function plus(p, q) {
  return added(p, q, 1);
}

function minus(p, q) {
  return added(p, q, -1);
}

// Every pair of terms, masks ORed and coefficients multiplied, summed by
// mask.
function times(p, q) {
  const n = q.masks.length;
  for (let j = 0; j < n; j++) {
    QH[j] = Math.floor(q.masks[j] / HALF);
    QL[j] = q.masks[j] % HALF;
  }
  for (let i = 0; i < p.masks.length; i++) {
    const a = p.masks[i];
    const ah = Math.floor(a / HALF) * HALF;
    const al = (a % HALF) * HALF;
    const c = p.coeffs[i];
    for (let j = 0; j < n; j++) {
      SUMS[OR7[ah + QH[j]] * HALF + OR7[al + QL[j]]] += c * q.coeffs[j];
    }
  }
  return fromSums(or14(unionOf(p), unionOf(q)));
}

// The value when the variables in `ones` are 1 and the rest 0.
function evalAt(p, ones) {
  let sum = 0;
  for (let i = 0; i < p.masks.length; i++) {
    const m = p.masks[i];
    if (and14(m, ones) === m) sum += p.coeffs[i];
  }
  return sum;
}

// The value at every point of `count` variables, written into `out` (an
// array of 2^count slots, reused): the coefficients summed over subsets,
// one variable at a time (the zeta transform).
function valuesInto(p, count, out) {
  const size = POW2[count];
  for (let m = 0; m < size; m++) out[m] = 0;
  for (let i = 0; i < p.masks.length; i++) out[p.masks[i]] += p.coeffs[i];
  for (let index = 0; index < count; index++) {
    const w = POW2[index];
    for (let base = 0; base < size; base += 2 * w) {
      for (let j = base + w; j < base + 2 * w; j++) out[j] += out[j - w];
    }
  }
  return out;
}

function degree(p) {
  let most = 0;
  for (const m of p.masks) most = Math.max(most, POP[m]);
  return most;
}

function maxAbsCoeff(p) {
  let most = 0;
  for (const c of p.coeffs) most = Math.max(most, Math.abs(c));
  return most;
}

// ------------------------------------------------------------ the stepper
//
// One step as four polynomials in six variables: hb, lb, halted, accepted,
// op_hb, op_lb. NOT x is 1 - x, x AND y is x·y, x OR y is x + y - x·y.

function not(x) {
  return minus(ONE_POLY, x);
}

function either(x, y) {
  return minus(plus(x, y), times(x, y));
}

function anyOf(polys) {
  let out = ZERO_POLY;
  for (const p of polys) out = either(out, p);
  return out;
}

function nextState(hb, lb, halted, accepted, opHb, opLb) {
  const is0 = times(not(hb), not(lb));
  const is1 = times(not(hb), lb);
  const is2 = times(hb, not(lb));
  const is3 = times(hb, lb);

  const runsPass = either(times(not(opHb), not(opLb)), halted);
  const runsCheck = times(times(not(opHb), opLb), not(halted));
  const runsDecr = times(times(opHb, not(opLb)), not(halted));
  const runsMod2 = times(times(opHb, opLb), not(halted));

  const none = ZERO_POLY;
  const pass = { to1: times(is1, runsPass), to2: times(is2, runsPass), to3: times(is3, runsPass), accepts: none, halts: none };
  const check = { to1: times(is1, runsCheck), to2: times(is2, runsCheck), to3: times(is3, runsCheck), accepts: times(is0, runsCheck), halts: none };
  const decr = { to1: times(is2, runsDecr), to2: times(is3, runsDecr), to3: none, accepts: none, halts: times(is0, runsDecr) };
  const mod2 = { to1: times(either(is3, is1), runsMod2), to2: none, to3: none, accepts: none, halts: none };
  const ops = [pass, check, decr, mod2];

  const becomes1 = anyOf(ops.map((op) => op.to1));
  const becomes2 = anyOf(ops.map((op) => op.to2));
  const becomes3 = anyOf(ops.map((op) => op.to3));
  return {
    hb: either(becomes3, becomes2),
    lb: either(becomes3, becomes1),
    halted: either(halted, anyOf(ops.map((op) => op.halts))),
    accepted: either(accepted, anyOf(ops.map((op) => op.accepts))),
  };
}

const STEP = nextState(variable(0), variable(1), variable(2), variable(3), variable(4), variable(5));
const STEP_POLYS = [STEP.hb, STEP.lb, STEP.halted, STEP.accepted];

// Each step polynomial's value at all 64 of its points, so a step of the
// stepper is four reads.
const STEP_TABLES = STEP_POLYS.map((p) => valuesInto(p, 6, []));

function bitOf(value) {
  if (value === 0 || value === 1) return value;
  throw new RangeError(`a step polynomial gave ${value}, not 0 or 1`);
}

// Python's `step`: the step polynomials at one state and op.
function stepperStep(s, op) {
  const ones = Math.floor(s.ax / 2) + 2 * (s.ax % 2) + 4 * s.halted + 8 * s.accepted + 16 * Math.floor(op / 2) + 32 * (op % 2);
  return {
    ax: 2 * bitOf(STEP_TABLES[0][ones]) + bitOf(STEP_TABLES[1][ones]),
    halted: bitOf(STEP_TABLES[2][ones]),
    accepted: bitOf(STEP_TABLES[3][ones]),
  };
}

function stepperRun(program, input) {
  let s = { ax: input, halted: 0, accepted: 0 };
  for (let i = 0; i < PROGRAM_LENGTH; i++) s = stepperStep(s, program[i]);
  return s;
}

// --------------------------------------------------------- find_solutions

function findSolutions() {
  const programs = [];
  for (let y = 0; y < LANGUAGE_COUNT; y++) programs.push([]);
  for (let n = 0; n < PROGRAM_COUNT; n++) programs[acceptedCode(disassemble(n))].push(n);
  for (let y = 0; y < LANGUAGE_COUNT; y++) {
    if (programs[y].length === 0) throw new Error(`no program accepts language ${y}`);
  }
  for (let y = 0; y < LANGUAGE_COUNT; y++) {
    const lang = language(y);
    console.log(`${langText(lang)} is solved by ${programs[y].length} possible program`);
    console.log("See an example program below.");
    console.log(`   it accepts ${langText(lang)}`);
    console.log(`   it rejects ${langText(complementOf(lang))}`);
    console.log("--");
    const example = disassemble(programs[y][0]);
    for (const op of example) console.log(`${OP_NAMES[op]} ${OP_COMMENTS[op]}`);
    console.log("--");
    console.log("");
    // Python's assert: the stepper accepts exactly the language.
    for (let input = 0; input < INPUT_COUNT; input++) {
      const accepted = stepperRun(example, input).accepted === 1;
      if (accepted !== lang.includes(input)) throw new Error(`the stepper disagrees on input ${input} of language ${y}`);
    }
  }
}

// ------------------------------------------------------------ composition
//
// The step polynomials substituted into themselves: after step k the state
// is four polynomials in 14 variables, the input's two bits (0, 1) and each
// instruction's two (2k, 2k + 1).

// The product of the replacements over each monomial of six variables,
// made once a step and kept by mask; null until made.
const PRODUCTS = [];
for (let m = 0; m < 64; m++) PRODUCTS.push(null);

function productOf(mask, replacements) {
  const made = PRODUCTS[mask];
  if (made !== null) return made;
  let product = ONE_POLY;
  if (mask > 0) {
    let top = 5;
    while (POW2[top] > mask) top--;
    product = times(productOf(mask - POW2[top], replacements), replacements[top]);
  }
  PRODUCTS[mask] = product;
  return product;
}

function substitute(p, replacements) {
  let width = 0;
  for (const r of replacements) width = or14(width, unionOf(r));
  // Every product first: a product is summed in SUMS too.
  for (const m of p.masks) productOf(m, replacements);
  for (let i = 0; i < p.masks.length; i++) {
    const product = productOf(p.masks[i], replacements);
    const c = p.coeffs[i];
    for (let j = 0; j < product.masks.length; j++) SUMS[product.masks[j]] += c * product.coeffs[j];
  }
  return fromSums(width);
}

function afterStep(before, index) {
  for (let m = 0; m < 64; m++) PRODUCTS[m] = null;
  const replacements = [before.hb, before.lb, before.halted, before.accepted, variable(2 * index), variable(2 * index + 1)];
  return {
    hb: substitute(STEP.hb, replacements),
    lb: substitute(STEP.lb, replacements),
    halted: substitute(STEP.halted, replacements),
    accepted: substitute(STEP.accepted, replacements),
  };
}

// The stepper's halted by its rule: rejected, or accepted with a `decr`
// after the accepting `zero`.
function haltsByRule(program, input) {
  let m = { ax: input, ending: RUNNING };
  let halts = 0;
  for (const op of program) {
    if (m.ending === ACCEPTED && op === DECR) halts = 1;
    else m = machineStep(m, op);
  }
  return m.ending === REJECTED ? 1 : halts;
}

// The values of the four final polynomials at every point, made once.
const VALUES = [[], [], [], []];

function proof(final) {
  const polys = [final.hb, final.lb, final.halted, final.accepted];
  for (let k = 0; k < 4; k++) valuesInto(polys[k], VARIABLE_LIMIT, VALUES[k]);
  let differ = 0;
  let accepted = 0;
  let unlike = 0;
  for (let ones = 0; ones < SIZE; ones++) {
    const input = 2 * (ones % 2) + (Math.floor(ones / 2) % 2);
    const program = [];
    let rest = Math.floor(ones / 4);
    for (let i = 0; i < PROGRAM_LENGTH; i++) {
      const hb = rest % 2;
      const lb = Math.floor(rest / 2) % 2;
      program.push(2 * hb + lb);
      rest = Math.floor(rest / 4);
    }
    const machine = machineRun(program, input);
    const stepper = stepperRun(program, input);
    const got = { hb: VALUES[0][ones], lb: VALUES[1][ones], halted: VALUES[2][ones], accepted: VALUES[3][ones] };
    const wantAccepted = machine.ending === ACCEPTED ? 1 : 0;
    let point = Math.floor(input / 2) + 2 * (input % 2);
    for (let at = 0; at < PROGRAM_LENGTH; at++) {
      const op = program[at];
      point += (Math.floor(op / 2) + 2 * (op % 2)) * POW2[2 * (at + 1)];
    }
    const agrees =
      got.accepted === wantAccepted &&
      got.hb === Math.floor(machine.ax / 2) &&
      got.lb === machine.ax % 2 &&
      got.halted === stepper.halted &&
      got.halted === haltsByRule(program, input) &&
      point === ones &&
      (ones % 61 !== 0 || evalAt(final.accepted, ones) === got.accepted);
    if (!agrees) differ++;
    accepted += wantAccepted;
    const haltedByMachine = machine.ending === RUNNING ? 0 : 1;
    if (got.halted !== haltedByMachine) unlike++;
  }
  return `proof: ${SIZE} points, ${differ} differ, ${accepted} accepted, ${unlike} accepted with no halt`;
}

function composition(count) {
  console.log("step  hb  lb  halted  accepted");
  let state = { hb: variable(0), lb: variable(1), halted: ZERO_POLY, accepted: ZERO_POLY };
  for (let index = 1; index <= count; index++) {
    state = afterStep(state, index);
    console.log(`${index}  ${state.hb.masks.length}  ${state.lb.masks.length}  ${state.halted.masks.length}  ${state.accepted.masks.length}`);
  }
  const named = [["hb", state.hb], ["lb", state.lb], ["halted", state.halted], ["accepted", state.accepted]];
  for (const [name, p] of named) {
    console.log(`${name}: ${p.masks.length} terms, degree ${degree(p)}, max |coeff| ${maxAbsCoeff(p)}`);
  }
  if (count === PROGRAM_LENGTH) console.log(proof(state));
  else console.log(`proof: needs all ${PROGRAM_LENGTH} steps`);
}

findSolutions();
composition(STEPS);
