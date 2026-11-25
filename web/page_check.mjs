// page_check: the Nibble playground, held to the C.
//
//   node web/page_check.mjs <site-dir> <puzzles.json> <runs.json> <screenshot.png>
//
// web/build.shale runs it on what it built. It:
//
// - loads the page's wasm (found by its hashed name in <site-dir>) in node
//   and holds every one of the 4,096 programs on every input to the C's
//   runs (web/puzzles.c's `runs`), both as `status` gives it and as six
//   `after` steps end;
// - counts, from the wasm alone, the programs that accept each language and
//   the length of the shortest (its ops before its trailing `nada`s), and
//   holds the C's 16-puzzle table to them and to its order: the four
//   lessons first (is_tiny; is_one and is_even, each with line 1 fixed, as
//   decr and mod2; grumpy, opening as zero, which nada, decr and mod2 each
//   solve), then the other twelve by effort (shortest length, a mod2 counted
//   as a line more; ties to no mod2, fewer zeros, more solutions), each
//   with its last line fixed as zero exactly when some shortest program ends
//   in zero; each example is a shortest and fits its open lines and its
//   fixed ones, each puzzle leaves a line to pick, and each start leaves it
//   unsolved;
// - serves <site-dir> on a loopback port and loads it in headless Chromium
//   (Playwright's, found by PLAYWRIGHT_BROWSERS_PATH, which Shale sets) at a
//   phone's width; there it sees the first lesson's sentence, its lines past
//   the first locked to nada and no Run button, reads which input the fresh
//   all-nada program gets wrong, then solves it by clicking (solved at the
//   last pick, not before), reads the trace and the count of programs, sees
//   that neither the page nor the trace scrolls sideways, and saves a
//   screenshot to <screenshot.png>. After a reload it sees puzzle 1 fresh,
//   only it open and nothing in localStorage; solves it again, then is_one
//   and is_even by one click each on line 2's zero (line 1 given and
//   locked), and grumpy by one click (nada; decr and mod2 too, zero not),
//   each opening the next; sees a solved puzzle reopen at its start; then
//   opens puzzles 5 to 16 in turn, each with its locks (its fixed zero among
//   them) and hint as it opens, and solves each by clicking its example's
//   open lines. Two more screenshots go beside the first: is_one with its
//   fixed line 1 (`-given` before `.png`), and is_odd with its fixed zero
//   (`-fixed`).
//
// The browser is started as roc-apps' tools/chromium.mjs starts it (its
// flags and its folder under TMPDIR, for Shale's confinement); this is a
// small copy, so the repository stands alone.
import { spawn } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { createServer } from "node:http";
import { tmpdir } from "node:os";
import { extname, join } from "node:path";

const [site, puzzlesFile, runsFile, shotFile] = process.argv.slice(2);
if (!shotFile) {
  console.error("usage: node web/page_check.mjs <site-dir> <puzzles.json> <runs.json> <screenshot.png>");
  process.exit(2);
}

let failures = 0;
let checks = 0;
function expect(ok, why) {
  checks++;
  if (!ok) { failures++; console.error(`page_check: FAIL ${why}`); }
}

const STEPS = 6;
const PROGRAMS = 4096;
// The lessons, by code (a member n is bit n), with their open lines, fixed
// lines (line k's bit 1 << k, from 0) and start: {0}; {1} and {0, 2}, line 1
// fixed as decr (2) and mod2 (3); { }, opening as zero (1).
const LESSONS = [
  { code: 1, name: "is_tiny", lines: 1, fixed: 0, start: 0 },
  { code: 2, name: "is_one", lines: 2, fixed: 1, start: 2 },
  { code: 5, name: "is_even", lines: 2, fixed: 1, start: 3 },
  { code: 0, name: "grumpy", lines: 1, fixed: 0, start: 1 },
];
const files = readdirSync(site);
const wasmFile = files.find((f) => /^nibble\.[0-9a-f]{16}\.wasm$/.test(f));
if (!wasmFile) { console.error(`page_check: no hashed wasm in ${site}`); process.exit(1); }

// ── the wasm against the C ───────────────────────────────────────────────
const { instance } = await WebAssembly.instantiate(readFileSync(join(site, wasmFile)));
const m = instance.exports;
const runs = JSON.parse(readFileSync(runsFile, "utf8"));
const table = JSON.parse(readFileSync(puzzlesFile, "utf8"));
expect(runs.length === PROGRAMS * 4, `runs.json holds ${runs.length} runs`);

const count = {};
const shortest = {};
let mismatches = 0;
for (let code = 0; code < PROGRAMS; code++) {
  let lang = 0;
  for (let n = 0; n < 4; n++) {
    const status = m.status(n, code);
    const stepped = (m.after(n, code, STEPS) >> 3) & 3;
    if (status !== Number(runs[code * 4 + n]) || stepped !== status) mismatches++;
    if (status === 1) lang |= 1 << n;
  }
  if (m.accepts(code) !== lang) mismatches++;
  let len = STEPS;
  while (len > 0 && Math.floor(code / 4 ** (len - 1)) % 4 === 0) len--;
  count[lang] = (count[lang] ?? 0) + 1;
  shortest[lang] = Math.min(shortest[lang] ?? STEPS + 1, len);
}
expect(mismatches === 0, `${mismatches} of ${PROGRAMS * 4} runs differ between the wasm and the C`);
// The languages some shortest program ends in zero for.
const endsInZero = new Set();
for (let code = 0; code < PROGRAMS; code++) {
  const lang = m.accepts(code);
  const len = shortest[lang];
  if (len > 0 && Math.floor(code / 4 ** len) === 0 && Math.floor(code / 4 ** (len - 1)) % 4 === 1) endsInZero.add(lang);
}

// Each language's plainest shortest program: fewest mod2s, then fewest zeros.
const plainest = {};
for (let code = 0; code < PROGRAMS; code++) {
  const lang = m.accepts(code);
  const len = shortest[lang];
  if (Math.floor(code / 4 ** len) !== 0) continue;
  let mod2s = 0, zeros = 0;
  for (let k = 0; k < len; k++) {
    const op = Math.floor(code / 4 ** k) % 4;
    mod2s += op === 3;
    zeros += op === 1;
  }
  const best = plainest[lang];
  if (!best || mod2s < best.mod2s || (mod2s === best.mod2s && zeros < best.zeros)) plainest[lang] = { mod2s, zeros };
}
// Past the lessons, easiest first: the shortest length with a mod2 counted as
// a line more; a tie to no mod2, then fewer zeros, then more solutions.
const effort = (code) => shortest[code] + plainest[code].mod2s;
const easierThan = (q, p) => [effort(q.code) - effort(p.code), plainest[q.code].mod2s - plainest[p.code].mod2s, plainest[q.code].zeros - plainest[p.code].zeros, p.count - q.count, q.code - p.code].find((d) => d !== 0) < 0;
// The order that comes to, by name, so a change to it is a change here too.
const RAMP = ["is_two", "is_small", "is_huge", "is_middling", "is_odd", "is_big", "is_extreme", "is_not_huge", "is_not_two", "always_true", "is_not_one", "is_not_tiny"];

// Hints past the lessons, beside the final zero: is_not_tiny ({1, 2, 3}, code
// 14) has line 2 given as mod2 (3 at base-4 digit 1), its fold.
const HINTED = { 14: { fixed: 1 << 1, start: 3 * 4, text: "Line 2 is already fixed as mod2, and line 5 as zero. Your job: fix lines 1, 3 and 4." } };

const puzzles = table.puzzles;
expect(puzzles.slice(4).map((p) => p.name).join(" ") === RAMP.join(" "), `the ramp after the lessons is ${puzzles.slice(4).map((p) => p.name).join(" ")}`);
const solves = (p, code) => m.accepts(code) === p.code;
const opAt = (code, k) => Math.floor(code / 4 ** k) % 4;
expect(puzzles.length === 16, `the table has ${puzzles.length} puzzles, not 16`);
expect(new Set(puzzles.map((p) => p.code)).size === 16 && puzzles.every((p) => p.code >= 0 && p.code < 16), "the table hasn't each of the 16 languages once");
for (let i = 0; i < puzzles.length; i++) {
  const p = puzzles[i];
  expect(p.kind === undefined && p.want === undefined, `${p.name}: an AX puzzle's field is left`);
  expect(p.count === count[p.code], `${p.name}: the C counts ${p.count} programs, the wasm ${count[p.code]}`);
  expect(p.shortest === shortest[p.code], `${p.name}: the C says shortest ${p.shortest}, the wasm ${shortest[p.code]}`);
  expect(solves(p, p.example), `${p.name}: its example program ${p.example} doesn't solve it`);
  // The page locks lines past `lines` to nada and the `fixed` ones to the
  // start's ops: the example, a shortest, must fit both.
  expect(p.given === undefined, `${p.name}: the old \`given\` field is left`);
  expect(Math.floor(p.example / 4 ** p.lines) === 0, `${p.name}: its example ${p.example} has an op past line ${p.lines}`);
  expect(Math.floor(p.example / 4 ** p.shortest) === 0, `${p.name}: its example ${p.example} isn't a shortest`);
  expect(p.fixed >> p.lines === 0, `${p.name}: a fixed line (${p.fixed}) is past its ${p.lines} lines`);
  let picks = 0;
  for (let k = 0; k < p.lines; k++) {
    if (((p.fixed >> k) & 1) === 0) { picks++; continue; }
    expect(opAt(p.example, k) === opAt(p.start, k), `${p.name}: its example's line ${k + 1} isn't the fixed one`);
    expect(opAt(p.start, k) !== 0, `${p.name}: its fixed line ${k + 1} is nada in its start`);
  }
  expect(picks > 0, `${p.name}: its fixed lines (${p.fixed}) of ${p.lines} leave none to pick`);
  // It opens unsolved, with nothing on a locked line.
  expect(!solves(p, p.start), `${p.name}: its start program ${p.start} solves it`);
  expect(Math.floor(p.start / 4 ** p.lines) === 0, `${p.name}: its start program ${p.start} has an op on a locked line`);
  expect((p.lesson !== undefined) === (i < LESSONS.length), `${p.name}: a lesson sentence only on the first four`);
  if (i < LESSONS.length) {
    const l = LESSONS[i];
    const got = `${p.code} ${p.name} ${p.lines} ${p.fixed} ${p.start}`;
    expect(got === `${l.code} ${l.name} ${l.lines} ${l.fixed} ${l.start}`, `puzzle ${i + 1} is \`${got}\`, not ${l.name}'s`);
  } else {
    // The hint: a shortest program's final zero, fixed, and nothing else.
    const [zeroFixed, zeroStart] = endsInZero.has(p.code) ? [1 << (p.lines - 1), 4 ** (p.lines - 1)] : [0, 0];
    const h = HINTED[p.code] ?? { fixed: 0, start: 0 };
    const [fixed, start] = [zeroFixed | h.fixed, zeroStart + h.start];
    expect(p.lines === p.shortest && p.fixed === fixed && p.start === start, `${p.name}: ${p.lines} lines, fixed ${p.fixed}, start ${p.start}, not ${p.shortest}, ${fixed}, ${start}`);
    if (i > LESSONS.length) {
      const q = puzzles[i - 1];
      expect(easierThan(q, p), `${p.name} comes after a harder puzzle, ${q.name}`);
    }
  }
}
// grumpy: nada, decr and mod2 each solve it alone on line 1; zero doesn't.
const grumpy = puzzles[3];
expect([0, 2, 3].every((op) => solves(grumpy, op)) && !solves(grumpy, 1), "grumpy isn't solved by nada, decr and mod2 alone, and not zero");
expect(table.ops.map((o) => o.name).join(" ") === "nada zero decr mod2", "the table's opcodes are nada zero decr mod2");

// ── the page, in Chromium ────────────────────────────────────────────────
const TYPES = { ".html": "text/html", ".js": "text/javascript", ".wasm": "application/wasm", ".json": "application/json" };
const server = createServer((req, res) => {
  const name = req.url === "/" ? "index.html" : decodeURIComponent(req.url.slice(1));
  if (!files.includes(name)) { res.writeHead(404); res.end(); return; }
  res.writeHead(200, { "content-type": TYPES[extname(name)] ?? "application/octet-stream" });
  res.end(readFileSync(join(site, name)));
});
await new Promise((up) => server.listen(0, "127.0.0.1", up));
const url = `http://127.0.0.1:${server.address().port}/`;

const sleep = (ms) => new Promise((done) => setTimeout(done, ms));
const BROWSERS = process.env.PLAYWRIGHT_BROWSERS_PATH || `${process.env.HOME}/.cache/ms-playwright`;
const chromium = (existsSync(BROWSERS) ? readdirSync(BROWSERS) : [])
  .filter((n) => /^chromium-\d+$/.test(n) && existsSync(`${BROWSERS}/${n}/chrome-linux/chrome`))
  .sort((a, b) => Number(b.slice(9)) - Number(a.slice(9)))[0];
if (!chromium) { console.error(`page_check: no Chromium in ${BROWSERS}`); process.exit(2); }

const folder = mkdtempSync(join(tmpdir(), "chromium-"));
mkdirSync(join(folder, "profile"));
mkdirSync(join(folder, "home", ".config"), { recursive: true });
const browser = spawn(`${BROWSERS}/${chromium}/chrome-linux/chrome`, [
  "--headless=new", "--no-sandbox", "--disable-gpu", "--hide-scrollbars", "--mute-audio",
  "--disable-dev-shm-usage", "--remote-debugging-port=0", `--user-data-dir=${join(folder, "profile")}`, "about:blank",
], { stdio: ["ignore", "ignore", "pipe"], env: { ...process.env, HOME: join(folder, "home"), XDG_CONFIG_HOME: join(folder, "home", ".config") } });
const address = await new Promise((found, refused) => {
  let said = "";
  browser.stderr.on("data", (chunk) => {
    said += chunk;
    const seen = said.match(/DevTools listening on (ws:\S+)/);
    if (seen) found(seen[1]);
  });
  browser.on("exit", (code) => refused(new Error(`chromium exited ${code}: ${said}`)));
});

const socket = new WebSocket(address);
await new Promise((open) => { socket.onopen = open; });
let lastId = 0;
const waiting = new Map();
const hearers = [];
socket.onmessage = ({ data }) => {
  const msg = JSON.parse(data);
  if (msg.id) {
    const { answer, refuse } = waiting.get(msg.id);
    waiting.delete(msg.id);
    msg.error ? refuse(new Error(JSON.stringify(msg.error))) : answer(msg.result);
  } else for (const hear of hearers) hear(msg);
};
const send = (method, params = {}, sessionId) => new Promise((answer, refuse) => {
  const id = ++lastId;
  waiting.set(id, { answer, refuse });
  socket.send(JSON.stringify({ id, method, params, sessionId }));
});

const { targetId } = await send("Target.createTarget", { url: "about:blank" });
const { sessionId } = await send("Target.attachToTarget", { targetId, flatten: true });
const page = (method, params) => send(method, params, sessionId);
const thrown = [];
let loaded = null;
hearers.push((msg) => {
  if (msg.sessionId !== sessionId) return;
  if (msg.method === "Runtime.exceptionThrown") thrown.push(msg.params.exceptionDetails.exception?.description ?? msg.params.exceptionDetails.text);
  if (msg.method === "Page.loadEventFired" && loaded) loaded();
});
await page("Page.enable");
await page("Runtime.enable");
const WIDTH = 390;
await page("Emulation.setDeviceMetricsOverride", { width: WIDTH, height: 1100, deviceScaleFactor: 1, mobile: true });

async function evaluate(expression) {
  const { result, exceptionDetails } = await page("Runtime.evaluate", { expression, awaitPromise: true, returnByValue: true });
  if (exceptionDetails) throw new Error(exceptionDetails.exception?.description ?? exceptionDetails.text);
  return result.value;
}
async function open(where) {
  const done = new Promise((d) => { loaded = d; });
  await page("Page.navigate", { url: where });
  await done;
  for (let k = 0; k < 50 && !(await evaluate("!!document.getElementById('trace')")); k++) await sleep(100);
}
const text = (id) => evaluate(`document.getElementById(${JSON.stringify(id)})?.textContent ?? null`);
// A real click, at the middle of the element the selector names.
async function click(selector) {
  const box = await evaluate(`(() => { const e = document.querySelector(${JSON.stringify(selector)}); if (!e) return null;
    e.scrollIntoView({ block: "center" }); const r = e.getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2 }; })()`);
  if (!box) throw new Error(`no ${selector} on the page`);
  for (const type of ["mousePressed", "mouseReleased"]) {
    await page("Input.dispatchMouseEvent", { type, x: box.x, y: box.y, button: "left", clickCount: 1 });
  }
  await sleep(30);
}
async function screenshot(file) {
  await evaluate("window.scrollTo(0, 0)");
  const { data } = await page("Page.captureScreenshot", { format: "png", captureBeyondViewport: true });
  writeFileSync(file, Buffer.from(data, "base64"));
}

const OPS = ["nada", "zero", "decr", "mod2"];
const first = puzzles[0];
const button = (i) => `document.querySelector('[data-puzzle="${i}"]')`;
const isSolvedMark = (i) => evaluate(`${button(i)}.classList.contains("solved")`);
const isShut = (i) => evaluate(`${button(i)}.disabled`);
// Each line as the page shows it: o (open), L (locked) or G (fixed: locked,
// its row marked given, the class the light-blue styling hangs on), then its
// highlighted op.
const locks = () => evaluate(`[...document.querySelectorAll(".slot")].map((r) => {
  const bs = [...r.querySelectorAll("button")], on = r.querySelector("button.on");
  const kind = bs.every((b) => b.disabled) ? (r.classList.contains("given") ? "G" : "L") : bs.some((b) => b.disabled) ? "?" : "o";
  return kind + (on ? on.dataset.op : "-"); }).join(" ")`);
const wantLocks = (p) => Array.from({ length: STEPS }, (_, k) => ((p.fixed >> k) & 1 ? "G" : k < p.lines ? "o" : "L") + OPS[opAt(p.start, k)]).join(" ");
const solvedNow = async () => (await text("verdict")).startsWith("Solved!");
try {
  await open(url);
  expect((await text("title")) === `Puzzle 1 of 16: ${first.name}`, `the first puzzle's title is \`${await text("title")}\``);
  expect((await text("hint")) === "This can be solved with a one-line Nibble program, so lines 2 to 6 are locked to nada.", `the first puzzle's hint says \`${await text("hint")}\``);
  // Each open button's tip says what its op does to each AX.
  const tips = await evaluate("['nada', 'zero', 'decr', 'mod2'].map((op) => document.querySelector(`[data-slot=\"0\"][data-op=\"${op}\"]`).title).join(' | ')");
  expect(tips === "nada: 0->0, 1->1, 2->2, 3->3 | zero: 0->A, 1->1, 2->2, 3->3 (A accepts) | decr: 0->R, 1->0, 2->1, 3->2 (R rejects) | mod2: 0->0, 1->1, 2->0, 3->1", `the buttons' tips are \`${tips}\``);
  expect(await isShut(1), "the second puzzle is open before the first is solved");
  expect((await text("lesson")) === first.lesson, `the first puzzle's lesson says \`${await text("lesson")}\``);
  expect((await locks()) === wantLocks(first), `the first puzzle's lines are \`${await locks()}\`, not \`${wantLocks(first)}\``);
  expect((await text("intro")).includes("The first four puzzles teach you Nibble"), "the intro doesn't say the first four puzzles teach Nibble");

  // No Run button: the fresh all-nada program has run already, and is wrong.
  expect(!(await evaluate("!!document.getElementById('run')")), "the page has a Run button");
  expect((await text("verdict")) === "Your program falsely rejects 0.", `the all-nada verdict says \`${await text("verdict")}\``);
  expect((await evaluate("document.querySelectorAll('#trace tbody tr').length")) === STEPS + 3, "the trace hasn't a row a step, then result, want and outcome");
  expect((await evaluate("document.querySelectorAll('#trace thead th[data-input]').length")) === 4, "the trace hasn't a column an input");

  // The C's example, clicked in slot by slot.
  for (let slot = 0; slot < STEPS; slot++) {
    const op = OPS[opAt(first.example, slot)];
    if (op === "nada") continue;
    expect(!(await solvedNow()), `solved before the pick on line ${slot + 1}`);
    await click(`[data-slot="${slot}"][data-op="${op}"]`);
  }
  expect(await solvedNow(), `the example's verdict says \`${await text("verdict")}\``);
  // The celebration shows without scrolling: the verdict's bottom is inside
  // the first screen, and it comes before the program's lines.
  const fold = await evaluate(`(() => { const v = document.getElementById("verdict").getBoundingClientRect(); return { bottom: v.bottom + window.scrollY, screen: window.innerHeight, before: !!(document.getElementById("verdict").compareDocumentPosition(document.querySelector(".slots")) & Node.DOCUMENT_POSITION_FOLLOWING) }; })()`);
  expect(fold.bottom <= fold.screen, `the verdict ends at ${Math.round(fold.bottom)}px, below the first ${fold.screen}px screen`);
  expect(fold.before, "the verdict doesn't come before the program's lines");
  expect(!(await evaluate("document.getElementById('intro').hidden")) && (await evaluate("document.getElementById('help').hidden")), "the first puzzle doesn't show the intro without a Help link");
  expect((await text("found")) === `You found one of ${first.count.toLocaleString("en-US")} programs that do this.`, `after solving the page says \`${await text("found")}\``);
  expect(!(await isShut(1)), "solving the first puzzle doesn't open the second");
  // No sideways scroll at a phone's width: not the page, and not the trace,
  // whose box would scroll on its own.
  const widths = await evaluate(`(() => { const s = document.scrollingElement, t = document.getElementById("trace"), tb = t.querySelector("table");
    return { page: s.scrollWidth, pageBox: s.clientWidth, trace: t.scrollWidth, traceBox: t.clientWidth, table: tb.scrollWidth, tableBox: tb.clientWidth }; })()`);
  expect(widths.page <= widths.pageBox, `at ${WIDTH}px the page is ${widths.page}px wide in ${widths.pageBox}px`);
  expect(widths.trace <= widths.traceBox, `at ${WIDTH}px the trace is ${widths.trace}px wide in its ${widths.traceBox}px box`);
  expect(widths.table <= widths.tableBox, `at ${WIDTH}px the trace's table is ${widths.table}px wide in ${widths.tableBox}px`);
  // The first puzzle is `zero`: step 1 accepts input 0, and leaves 1 as AX=1.
  const step1 = await evaluate("[...document.querySelectorAll('#trace tr[data-step=\"1\"] td')].map((c) => c.textContent).join(\" \")");
  expect(step1 === "accept AX=1 AX=2 AX=3", `the trace's first step shows \`${step1}\``);
  await screenshot(shotFile);

  // A reload forgets it all: puzzle 1 fresh, only it open, nothing stored.
  await open(url);
  expect((await text("title")) === `Puzzle 1 of 16: ${first.name}`, `after a reload the page shows \`${await text("title")}\``);
  expect(!(await solvedNow()) && !(await isSolvedMark(0)), "after a reload the first puzzle is solved");
  expect((await locks()) === wantLocks(first), `after a reload the first puzzle's lines are \`${await locks()}\``);
  let shut = true;
  for (let i = 1; i < puzzles.length; i++) shut = shut && (await isShut(i));
  expect(shut, "after a reload a puzzle past the first is open");
  expect((await evaluate("localStorage.length")) === 0, `localStorage holds ${await evaluate("localStorage.length")} items`);

  // Solved again, then the fixed-line lessons, each by one click on line
  // 2's zero, line 1 fixed and locked.
  await click('[data-slot="0"][data-op="zero"]');
  expect(await solvedNow(), "zero on line 1 doesn't solve the first puzzle");
  const given = [[1, "decr", "Your program falsely rejects 1."],
    [2, "mod2", "Your program falsely rejects 0 and 2."]];
  for (const [i, op, opening] of given) {
    await click("#next");
    const p = puzzles[i];
    expect((await text("title")) === `Puzzle ${i + 1} of 16: ${p.name}`, `Next shows \`${await text("title")}\`, not ${p.name}`);
    expect((await locks()) === wantLocks(p), `${p.name}'s lines are \`${await locks()}\`, not \`${wantLocks(p)}\``);
    expect((await locks()).startsWith(`G${op} onada L`), `${p.name}'s line 1 isn't fixed as ${op}`);
    expect((await evaluate(`document.querySelector('[data-slot="0"][data-op="${op}"]').title`)) === `Fixed: line 1 is ${op}.`, `${p.name}'s fixed line's title isn't \`Fixed: line 1 is ${op}.\``);
    expect((await text("hint")) === `Line 1 is already fixed as ${op}. Your job: fix line 2. Lines 3 to 6 are locked to nada.`, `${p.name}'s hint says \`${await text("hint")}\``);
    // The fixed op is shown light, as the locked nadas are: the locked rows' opacity.
    const shade = await evaluate(`[getComputedStyle(document.querySelector('[data-slot="0"][data-op="${op}"]')).opacity, getComputedStyle(document.querySelector('[data-slot="3"][data-op="nada"]')).opacity].join(" ")`);
    expect(shade.split(" ")[0] === shade.split(" ")[1] && Number(shade.split(" ")[0]) < 1, `${p.name}'s fixed ${op} isn't light like the locked nadas: opacities ${shade}`);
    expect((await text("lesson")) === p.lesson, `${p.name}'s lesson is \`${await text("lesson")}\``);
    expect((await text("verdict")) === opening, `${p.name} as it opens says \`${await text("verdict")}\``);
    if (i === 1) {
      await screenshot(shotFile.replace(/\.png$/, "-given.png"));
      // Past the first puzzle the intro folds away behind Help, which opens
      // and closes it.
      expect((await evaluate("document.getElementById('intro').hidden")) && !(await evaluate("document.getElementById('help').hidden")), "the second puzzle shows the intro, or no Help link");
      await click("#help");
      expect(!(await evaluate("document.getElementById('intro').hidden")) && (await text("help")) === "Hide help", "Help doesn't open the intro");
      await click("#help");
      expect((await evaluate("document.getElementById('intro').hidden")) && (await text("help")) === "Help", "Hide help doesn't close the intro");
    }
    await click('[data-slot="1"][data-op="zero"]');
    expect(await solvedNow(), `after zero on line 2 ${p.name} says \`${await text("verdict")}\``);
    expect((await isSolvedMark(i)) && !(await isShut(i + 1)), `solving ${p.name} doesn't mark it and open the next`);
  }

  // grumpy: one line, opening as zero, which accepts 0; nada solves it in
  // one click, and so do decr and mod2, but not zero.
  await click("#next");
  expect((await text("title")) === "Puzzle 4 of 16: grumpy", `the fourth puzzle's title is \`${await text("title")}\``);
  expect((await text("goal")) === "Reject all inputs.", `grumpy's goal is \`${await text("goal")}\``);
  expect((await locks()) === wantLocks(grumpy), `grumpy's lines are \`${await locks()}\``);
  expect((await text("lesson")) === grumpy.lesson, `grumpy's sentence is \`${await text("lesson")}\``);
  expect((await text("verdict")) === "Your program falsely accepts 0.", `grumpy as it opens says \`${await text("verdict")}\``);
  await click('[data-slot="0"][data-op="nada"]');
  expect(await solvedNow(), `after nada on line 1 grumpy says \`${await text("verdict")}\``);
  expect((await isSolvedMark(3)) && !(await isShut(4)), "solving grumpy doesn't mark it and open the next");
  // Solved, the buttons fold away and the trace's outcome row cheers; "Change
  // the program" brings the buttons back.
  expect((await evaluate("document.getElementById('slots').hidden")) && !(await evaluate("document.getElementById('edit').hidden")), "solving grumpy doesn't fold the buttons away behind Change the program");
  const cheers = await evaluate("[...document.querySelectorAll('#trace tr[data-row=\"outcome\"] td')].map((c) => c.textContent).join(\" | \")");
  expect(cheers === "Correct! 🎉 | Correct! 🎉 | Correct! 🎉 | Correct! 🎉", `grumpy's outcome row says \`${cheers}\``);
  await click("#edit");
  expect(!(await evaluate("document.getElementById('slots').hidden")) && (await evaluate("document.getElementById('edit').hidden")), "Change the program doesn't bring the buttons back");
  for (const op of ["decr", "mod2"]) {
    await click(`[data-slot="0"][data-op="${op}"]`);
    expect(await solvedNow(), `${op} on line 1 doesn't solve grumpy`);
  }
  await click('[data-slot="0"][data-op="zero"]');
  expect(!(await solvedNow()), "zero on line 1 solves grumpy");
  await click("#next");
  expect((await text("title")) === `Puzzle 5 of 16: ${puzzles[4].name}`, `after grumpy Next shows \`${await text("title")}\``);
  // Wrong both ways, every input named: zero on is_two's line 1 accepts 0,
  // and its fixed zero on line 3 never meets AX at 0 again, so 2 is rejected.
  await click('[data-slot="0"][data-op="zero"]');
  expect((await text("verdict")) === "Your program falsely accepts 0. It falsely rejects 2.", `zero on is_two says \`${await text("verdict")}\``);
  const marks = await evaluate("[...document.querySelectorAll('#trace tr[data-row=\"outcome\"] td')].map((c) => c.textContent).join(\" | \")");
  expect(marks === "WRONG! ❌ | Correct! 🎉 | WRONG! ❌ | Correct! 🎉", `zero on is_two's outcome row says \`${marks}\``);
  // At a phone's width each outcome cell holds its words without clipping.
  const clipped = await evaluate("[...document.querySelectorAll('#trace tr[data-row=\"outcome\"] td')].filter((c) => c.scrollWidth > c.clientWidth).length");
  expect(clipped === 0, `${clipped} of the outcome row's cells are clipped at ${WIDTH}px`);
  // The machine code follows every pick: is_two's zero on line 1 and its
  // fixed zero on line 3, two bits a line, line 1 first, and its number.
  expect((await text("machine-code")) === "01 00 01 00 00 00  (#17)", `is_two's machine code reads \`${await text("machine-code")}\``);
  await click('[data-slot="0"][data-op="decr"]');
  expect((await text("machine-code")) === "10 00 01 00 00 00  (#18)", `after decr on line 1 the machine code reads \`${await text("machine-code")}\``);
  // Its "?" opens a line about the bits, and closes it again.
  expect(await evaluate("document.getElementById('code-explain').hidden"), "the machine code's explanation shows before its ? is pressed");
  await click("#code-help");
  expect(!(await evaluate("document.getElementById('code-explain').hidden")) && (await text("code-explain")).startsWith("The machine code:"), "the machine code's ? doesn't open its explanation");
  await click("#code-help");
  expect(await evaluate("document.getElementById('code-explain').hidden"), "the machine code's ? doesn't close its explanation");
  const sideways = await evaluate("document.scrollingElement.scrollWidth > document.scrollingElement.clientWidth");
  expect(!sideways, `with the machine code beside the buttons the page scrolls sideways at ${WIDTH}px`);

  // A solved puzzle reopens as a puzzle: unsolved, at its start, still
  // marked solved in the row.
  await click('[data-puzzle="1"]');
  expect(!(await solvedNow()) && (await text("found")) === "" && (await locks()) === wantLocks(puzzles[1]), `the solved is_one reopens solved: \`${await text("verdict")}\``);
  expect(await isSolvedMark(1), "reopening is_one unmarks it");

  // Puzzles 5 to 16 in turn: each as it opens, its locks (the fixed zero
  // among them, light like a lesson's fixed line) and its hint; then solved
  // by clicking its example's open lines, which opens the next.
  const LINES = ["an empty", "a one-line", "a two-line", "a three-line", "a four-line", "a five-line", "a six-line"];
  const span = (a, b) => (a === b ? `line ${a}` : `lines ${a} to ${b}`);
  const lockedText = (n) => (n === STEPS ? "" : ` ${n + 1 === STEPS ? "Line" : "Lines"} ${n + 1 === STEPS ? n + 1 : `${n + 1} to ${STEPS}`} ${n + 1 === STEPS ? "is" : "are"} locked to nada.`);
  let fixedSolved = 0;
  await click(`[data-puzzle="${LESSONS.length}"]`);
  for (let i = LESSONS.length; i < puzzles.length; i++) {
    const p = puzzles[i];
    expect((await text("title")) === `Puzzle ${i + 1} of 16: ${p.name}`, `puzzle ${i + 1} shows \`${await text("title")}\`, not ${p.name}`);
    expect((await locks()) === wantLocks(p), `${p.name}'s lines are \`${await locks()}\`, not \`${wantLocks(p)}\``);
    expect(!(await solvedNow()), `${p.name} opens solved`);
    const n = p.lines;
    const hint = HINTED[p.code]
      ? `This can be solved with ${LINES[n]} Nibble program. ${HINTED[p.code].text}${lockedText(n)}`
      : p.fixed
      ? `This can be solved with ${LINES[n]} Nibble program. Line ${n} is already fixed as zero. Your job: fix ${span(1, n - 1)}.${lockedText(n)}`
      : `This can be solved with ${LINES[n]} Nibble program${n === STEPS ? "" : `, so ${span(n + 1, STEPS)} ${n + 1 === STEPS ? "is" : "are"} locked to nada`}.`;
    expect((await text("hint")) === hint, `${p.name}'s hint says \`${await text("hint")}\`, not \`${hint}\``);
    if (p.fixed) {
      const last = `[data-slot="${n - 1}"][data-op="zero"]`;
      expect((await evaluate(`document.querySelector('${last}').title`)) === `Fixed: line ${n} is zero.`, `${p.name}'s fixed zero's title is \`${await evaluate(`document.querySelector('${last}').title`)}\``);
      const look = await evaluate(`(() => { const b = document.querySelector('${last}'); return [b.classList.contains("on"), b.disabled, getComputedStyle(b).opacity, b.closest(".slot").classList.contains("given")].join(" "); })()`);
      expect(look === "true true 0.45 true", `${p.name}'s fixed zero isn't drawn as a fixed line (on, locked, light): \`${look}\``);
      if (i === LESSONS.length) await screenshot(shotFile.replace(/\.png$/, "-fixed.png"));
    }
    for (let k = 0; k < n; k++) {
      const op = OPS[opAt(p.example, k)];
      if (op === "nada" || (p.fixed >> k) & 1 || (await solvedNow())) continue;
      await click(`[data-slot="${k}"][data-op="${op}"]`);
    }
    expect(await solvedNow(), `${p.name}'s example, clicked in, says \`${await text("verdict")}\``);
    if (p.fixed && (await solvedNow())) fixedSolved++;
    if (i + 1 < puzzles.length) await click("#next");
  }
  expect(fixedSolved === puzzles.filter((p, i) => i >= LESSONS.length && p.fixed).length, `${fixedSolved} fixed-zero puzzles solved by clicks`);
  expect((await evaluate("localStorage.length")) === 0, "the page stored something in localStorage");
  expect(thrown.length === 0, `the page threw: ${thrown.join("; ")}`);
} finally {
  socket.close();
  browser.removeAllListeners("exit");
  browser.kill("SIGKILL");
  server.close();
  await sleep(300);
  rmSync(folder, { recursive: true, force: true });
}

if (failures > 0) { console.error(`page_check: ${failures} of ${checks} checks failed`); process.exit(1); }
const hardest = puzzles[puzzles.length - 1];
const fixedCount = puzzles.filter((p, i) => i >= LESSONS.length && p.fixed).length;
console.log(`ok   page_check: the wasm matches the C on all ${PROGRAMS * 4} runs and its ${puzzles.length}-puzzle table (${first.name} ${first.count} programs, shortest ${first.shortest}; ... ${hardest.name} ${hardest.count}, shortest ${hardest.shortest}); in Chromium at ${WIDTH}px with no sideways scroll the first puzzle was solved by clicks, a reload started fresh with nothing stored, is_one and is_even were solved by line 2's zero and grumpy by one click, and puzzles 5 to 16 opened with their locks and were solved by clicks, ${fixedCount} of them with a fixed zero (${checks} checks)`);
