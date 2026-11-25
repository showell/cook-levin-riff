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
// - counts, from the wasm alone, the programs that accept each language,
//   and those that give each AX puzzle's outcomes (accept, reject, or AX=n
//   off the end, AX read from six `after` steps), and the length of the
//   shortest (its ops before its trailing `nada`s), and holds the C's
//   19-puzzle table to them, to the AX puzzles' wants, and to its order:
//   the five one-line lessons first (zero, decr, mod2, nada, the nada
//   example), then by shortest length and count; each example fits in its
//   choosable lines, and each puzzle's start leaves it unsolved;
// - serves <site-dir> on a loopback port and loads it in headless Chromium
//   (Playwright's, found by PLAYWRIGHT_BROWSERS_PATH, which Shale sets) at a
//   phone's width; there it sees the first lesson's sentence and its lines
//   past the shortest locked to nada and no Run button, reads which input
//   the fresh all-nada program gets wrong, then solves it by clicking each
//   slot's opcode (solved at the last pick, not before), reads the trace
//   and the count of programs, sees that neither the
//   page nor the trace scrolls sideways, sees the second puzzle (an AX
//   puzzle) open with its want row and AX verdict, and after a reload sees
//   the first still solved. Then it loads an older save (a place,
//   `current`, from the old order, and languages solved by code) and sees
//   the solved languages kept, solves the three AX lessons by one click
//   each, and sees the nada lesson open unsolved (`zero` on its one line)
//   and solved by one pick, nada.
//   It saves a screenshot to <screenshot.png>.
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
// The lessons, by code: {0} (a member n is bit n), the AX puzzles one_less,
// last_bit and unchanged (16 to 18), and { }.
const LESSONS = [1, 16, 17, 18, 0];
// The AX puzzles' wants, for inputs 0 to 3.
const AX_WANTS = { 16: "reject AX=0 AX=1 AX=2", 17: "AX=0 AX=1 AX=0 AX=1", 18: "AX=0 AX=1 AX=2 AX=3" };
// The two puzzles all nada solves, each one line.
const NADA_SOLVES = [18, 0];
const files = readdirSync(site);
const wasmFile = files.find((f) => /^nibble\.[0-9a-f]{16}\.wasm$/.test(f));
if (!wasmFile) { console.error(`page_check: no hashed wasm in ${site}`); process.exit(1); }

// ── the wasm against the C ───────────────────────────────────────────────
const { instance } = await WebAssembly.instantiate(readFileSync(join(site, wasmFile)));
const m = instance.exports;
const runs = JSON.parse(readFileSync(runsFile, "utf8"));
const table = JSON.parse(readFileSync(puzzlesFile, "utf8"));
expect(runs.length === PROGRAMS * 4, `runs.json holds ${runs.length} runs`);

// A program's outcome on each input, from the wasm alone: accept, reject,
// or AX=n for a run off the end.
const outcomes = (code) => [0, 1, 2, 3].map((n) => {
  const w = m.after(n, code, STEPS), status = (w >> 3) & 3;
  return status === 1 ? "accept" : status === 2 ? "reject" : `AX=${w & 3}`;
}).join(" ");
const count = {};
const shortest = {};
const tally = (code, len) => { count[code] = (count[code] ?? 0) + 1; shortest[code] = Math.min(shortest[code] ?? STEPS + 1, len); };
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
  tally(lang, len);
  const outs = outcomes(code);
  for (const [ax, want] of Object.entries(AX_WANTS)) if (outs === want) tally(Number(ax), len);
}
expect(mismatches === 0, `${mismatches} of ${PROGRAMS * 4} runs differ between the wasm and the C`);

const puzzles = table.puzzles;
// Whether program `code` solves puzzle `p`, by the wasm.
const solves = (p, code) => (p.kind === "ax" ? outcomes(code) === p.want.join(" ") : m.accepts(code) === p.code);
const languages = puzzles.filter((p) => p.kind === "language");
const axes = puzzles.filter((p) => p.kind === "ax");
expect(puzzles.length === 19, `the table has ${puzzles.length} puzzles, not 19`);
expect(languages.length === 16 && new Set(languages.map((p) => p.code)).size === 16 && languages.every((p) => p.code >= 0 && p.code < 16), "the table hasn't each of the 16 languages once");
expect(axes.map((p) => p.code).sort().join(" ") === "16 17 18", `the AX puzzles' codes are ${axes.map((p) => p.code).join(" ")}`);
for (const p of axes) expect(p.want.join(" ") === AX_WANTS[p.code], `${p.name}: the C wants ${p.want.join(" ")}`);
for (let i = 0; i < puzzles.length; i++) {
  const p = puzzles[i];
  expect(p.count === count[p.code], `${p.name}: the C counts ${p.count} programs, the wasm ${count[p.code]}`);
  expect(p.shortest === shortest[p.code], `${p.name}: the C says shortest ${p.shortest}, the wasm ${shortest[p.code]}`);
  expect(solves(p, p.example), `${p.name}: its example program ${p.example} doesn't solve it`);
  // All nada solves only unchanged and { }.
  expect(solves(p, 0) === NADA_SOLVES.includes(p.code), `${p.name}: the all-nada program ${solves(p, 0) ? "solves" : "doesn't solve"} it`);
  // The page locks lines past `lines` to nada: a solution must fit. Only
  // the two all nada solves have more lines than their shortest, one.
  expect(p.lines === (NADA_SOLVES.includes(p.code) ? 1 : p.shortest), `${p.name}: ${p.lines} choosable lines, shortest ${p.shortest}`);
  expect(Math.floor(p.example / 4 ** p.lines) === 0, `${p.name}: its example ${p.example} has an op past line ${p.lines}`);
  // It opens unsolved, with nothing on a locked line.
  expect(!solves(p, p.start), `${p.name}: its start program ${p.start} solves it`);
  expect(Math.floor(p.start / 4 ** p.lines) === 0, `${p.name}: its start program ${p.start} has an op on a locked line`);
  expect((p.lesson !== undefined) === (i < LESSONS.length), `${p.name}: a lesson sentence only on the first five`);
  if (i < LESSONS.length) expect(p.code === LESSONS[i], `puzzle ${i + 1} is ${p.name}, not lesson ${i + 1}`);
  else if (i > LESSONS.length) {
    const q = puzzles[i - 1];
    expect(q.shortest < p.shortest || (q.shortest === p.shortest && q.count >= p.count), `${p.name} comes after a harder puzzle, ${q.name}`);
  }
}
expect(puzzles.slice(0, 5).every((p) => p.lines === 1), `the lessons' lines are ${puzzles.slice(0, 5).map((p) => p.lines).join(" ")}, not all one`);
expect(puzzles.slice(0, 5).map((p) => p.shortest).join(" ") === "1 1 1 0 0", `the lessons' shortest lengths are ${puzzles.slice(0, 5).map((p) => p.shortest).join(" ")}`);
expect(puzzles.slice(5, 7).map((p) => p.code).join(" ") === "5 2", `after the lessons come ${puzzles.slice(5, 7).map((p) => p.name).join(", ")}, not {0, 2} and {1}`);
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

const OPS = ["nada", "zero", "decr", "mod2"];
const first = puzzles[0];
const LINES = ["an empty", "a one-line", "a two-line", "a three-line", "a four-line", "a five-line", "a six-line"];
try {
  await open(url);
  expect((await text("title")) === `Puzzle 1 of ${puzzles.length}: ${first.name}`, `the first puzzle's title is \`${await text("title")}\``);
  expect((await text("hint")).includes(`${LINES[first.shortest]} Nibble program`), `the first puzzle's hint says \`${await text("hint")}\``);
  expect((await evaluate("document.querySelector('[data-slot=\"0\"][data-op=\"zero\"]').title")) === table.ops[1].comment, "zero's tooltip isn't its comment");
  expect(await evaluate("document.querySelector('[data-puzzle=\"1\"]').disabled"), "the second puzzle is open before the first is solved");
  expect((await text("lesson")) === first.lesson, `the first puzzle's lesson says \`${await text("lesson")}\``);
  // Lines past the shortest length are locked to nada, and every line starts as nada.
  const locks = async (p) => evaluate(`[...document.querySelectorAll(".slot")].map((r) => {
    const bs = [...r.querySelectorAll("button")], on = r.querySelector("button.on");
    return (bs.every((b) => b.disabled) ? "L" : bs.some((b) => b.disabled) ? "?" : "o") + (on ? on.dataset.op : "-"); }).join(" ")`);
  const wantLocks = (p) => Array.from({ length: STEPS }, (_, k) => (k < p.lines ? "o" : "L") + OPS[Math.floor(p.start / 4 ** k) % 4]).join(" ");
  expect((await locks()) === wantLocks(first), `the first puzzle's lines are \`${await locks()}\`, not \`${wantLocks(first)}\``);

  // No Run button: the fresh all-nada program has run already, and is wrong.
  expect(!(await evaluate("!!document.getElementById('run')")), "the page has a Run button");
  const wrongAt = first.set[0];
  expect((await text("verdict")) === `Input ${wrongAt} should be accepted, and your program rejected it.`, `the all-nada verdict says \`${await text("verdict")}\``);
  expect((await evaluate("document.querySelectorAll('#trace tbody tr').length")) === STEPS + 2, "the trace hasn't a row a step, then result and want");
  expect((await evaluate("document.querySelectorAll('#trace thead th[data-input]').length")) === 4, "the trace hasn't a column an input");

  // The C's example, clicked in slot by slot.
  for (let slot = 0; slot < STEPS; slot++) {
    const op = OPS[Math.floor(first.example / 4 ** slot) % 4];
    if (op === "nada") continue;
    expect(!(await text("verdict")).startsWith("Solved!"), `solved before the pick on line ${slot + 1}`);
    await click(`[data-slot="${slot}"][data-op="${op}"]`);
  }
  expect((await text("verdict")).startsWith("Solved!"), `the example's verdict says \`${await text("verdict")}\``);
  // The celebration shows without scrolling: the verdict's bottom is inside
  // the first screen, and it comes before the program's lines.
  const fold = await evaluate(`(() => { const v = document.getElementById("verdict").getBoundingClientRect(); return { bottom: v.bottom + window.scrollY, screen: window.innerHeight, before: !!(document.getElementById("verdict").compareDocumentPosition(document.querySelector(".slots")) & Node.DOCUMENT_POSITION_FOLLOWING) }; })()`);
  expect(fold.bottom <= fold.screen, `the verdict ends at ${Math.round(fold.bottom)}px, below the first ${fold.screen}px screen`);
  expect(fold.before, "the verdict doesn't come before the program's lines");
  expect(!(await evaluate("document.getElementById('intro').hidden")) && (await evaluate("document.getElementById('help').hidden")), "the first puzzle doesn't show the intro without a Help link");
  expect((await text("found")) === `You found one of ${first.count.toLocaleString("en-US")} programs that do this.`, `after solving the page says \`${await text("found")}\``);
  expect(!(await evaluate("document.querySelector('[data-puzzle=\"1\"]').disabled")), "solving the first puzzle doesn't open the second");
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
  await evaluate("window.scrollTo(0, 0)");
  const { data } = await page("Page.captureScreenshot", { format: "png", captureBeyondViewport: true });
  writeFileSync(shotFile, Buffer.from(data, "base64"));

  // The second puzzle, by Next, then a reload: the progress kept. It is an
  // AX puzzle, one_less: the all-nada program ends each input with AX
  // where it started, and the want row says how each should end.
  await click("#next");
  expect((await text("title")) === `Puzzle 2 of ${puzzles.length}: ${puzzles[1].name}`, `Next shows \`${await text("title")}\``);
  expect((await locks()) === wantLocks(puzzles[1]), `the second puzzle's lines are \`${await locks()}\``);
  expect((await text("goal")) === puzzles[1].goal, `the second puzzle's goal is \`${await text("goal")}\``);
  expect((await text("lesson")) === puzzles[1].lesson, `the second puzzle's lesson is \`${await text("lesson")}\``);
  expect((await text("verdict")) === "Input 0 should be rejected; your program ended with AX=0.", `the all-nada one_less verdict says \`${await text("verdict")}\``);
  const endRows = async () => evaluate(`["result", "want"].map((r) => [...document.querySelectorAll('#trace tr[data-row="' + r + '"] td')].map((c) => c.textContent).join(" ")).join(" / ")`);
  expect((await endRows()) === "AX=0 AX=1 AX=2 AX=3 / reject AX=0 AX=1 AX=2", `one_less's last two rows are \`${await endRows()}\``);
  // Past the first puzzle the intro folds away behind Help, which opens and
  // closes it.
  expect((await evaluate("document.getElementById('intro').hidden")) && !(await evaluate("document.getElementById('help').hidden")), "the second puzzle shows the intro, or no Help link");
  await click("#help");
  expect(!(await evaluate("document.getElementById('intro').hidden")) && (await text("help")) === "Hide help", "Help doesn't open the intro");
  await click("#help");
  expect((await evaluate("document.getElementById('intro').hidden")) && (await text("help")) === "Help", "Hide help doesn't close the intro");
  await open(url);
  expect(await evaluate("document.querySelector('[data-puzzle=\"0\"]').classList.contains('solved')"), "after a reload the first puzzle isn't marked solved");
  expect((await text("title")) === `Puzzle 2 of ${puzzles.length}: ${puzzles[1].name}`, `after a reload the page shows \`${await text("title")}\``);
  // A solved puzzle reopens as a puzzle: unsolved, at its start, still
  // marked solved in the row.
  await click('[data-puzzle="0"]');
  expect(!(await text("verdict")).startsWith("Solved!") && (await text("found")) === "", `the solved first puzzle reopens solved: \`${await text("verdict")}\``);
  expect(await evaluate("document.querySelector('[data-puzzle=\"0\"]').classList.contains('solved')"), "reopening the first puzzle unmarks it");
  await click('[data-puzzle="1"]');

  // An older save, from before the AX puzzles: its puzzle by place
  // (`current`) is dropped, and its solved languages, by code ({0} and
  // {2}), stay solved and open wherever they now stand; the AX lesson after
  // {0} opens, unsolved, and the one after it stays shut.
  const at = (code) => puzzles.findIndex((p) => p.code === code);
  const button = (i) => `document.querySelector('[data-puzzle="${i}"]')`;
  const twoAt = at(4);
  const nadaAt = at(0);
  await evaluate(`localStorage.setItem("nibble-progress-v1", JSON.stringify({ solved: { "1": [1, 0, 0, 0, 0, 0], "4": [2, 2, 1, 0, 0, 0] }, current: 3, drafts: { "0": [1, 2, 3, 1, 2, 3] } }))`);
  await open(url);
  expect((await text("title")) === `Puzzle 1 of ${puzzles.length}: ${first.name}`, `an old save's place shows \`${await text("title")}\``);
  expect(await evaluate(`${button(0)}.classList.contains("solved")`), "an old save's solved {0} isn't solved");
  expect(await evaluate(`(() => { const b = ${button(twoAt)}; return b.classList.contains("solved") && !b.disabled; })()`), "an old save's solved {2} isn't solved and open");
  expect(await evaluate(`(() => { const b = ${button(1)}; return !b.classList.contains("solved") && !b.disabled; })()`), "after an old save one_less isn't open and unsolved");
  expect(await evaluate(`${button(2)}.disabled`), "after an old save last_bit is open");

  // The three AX lessons, each solved by one click on line 1, each opening
  // the next: decr, mod2, then nada over unchanged's `mod2`.
  const axLessons = [[1, "decr", "Input 0 should be rejected; your program ended with AX=0."],
    [2, "mod2", "Input 2 should end with AX=0; your program ended with AX=2."],
    [3, "nada", "Input 2 should end with AX=2; your program ended with AX=0."]];
  for (const [i, op, opening] of axLessons) {
    await click(`[data-puzzle="${i}"]`);
    const p = puzzles[i];
    expect((await text("title")) === `Puzzle ${i + 1} of ${puzzles.length}: ${p.name}`, `AX lesson ${i + 1}'s title is \`${await text("title")}\``);
    expect((await locks()) === wantLocks(p), `${p.name}'s lines are \`${await locks()}\`, not \`${wantLocks(p)}\``);
    expect((await text("hint")).includes("a one-line Nibble program, so lines 2 to 6 are locked to nada"), `${p.name}'s hint says \`${await text("hint")}\``);
    expect((await text("verdict")) === opening, `${p.name} as it opens says \`${await text("verdict")}\``);
    await click(`[data-slot="0"][data-op="${op}"]`);
    expect((await text("verdict")).startsWith("Solved!"), `after ${op} on line 1 ${p.name} says \`${await text("verdict")}\``);
    expect((await endRows()) === `${p.want.join(" ")} / ${p.want.join(" ")}`, `${p.name} solved, its last two rows are \`${await endRows()}\``);
    expect(await evaluate(`${button(i)}.classList.contains("solved") && !${button(i + 1)}.disabled`), `solving ${p.name} doesn't mark it and open the next`);
  }

  // The nada lesson, opened by unchanged: one line, which opens as zero, so
  // it opens unsolved, and its locked lines stay nada over a saved draft;
  // one pick, nada, solves it.
  expect(nadaAt === 4, `the nada example is puzzle ${nadaAt + 1}`);
  await click(`[data-puzzle="${nadaAt}"]`);
  expect((await text("title")) === `Puzzle ${nadaAt + 1} of ${puzzles.length}: always_false`, `the nada lesson's title is \`${await text("title")}\``);
  expect((await locks()) === wantLocks(puzzles[nadaAt]), `the nada lesson's lines are \`${await locks()}\``);
  expect((await text("hint")).includes("a one-line Nibble program, so lines 2 to 6 are locked to nada"), `the nada lesson's hint says \`${await text("hint")}\``);
  expect((await text("lesson")) === puzzles[nadaAt].lesson, `the nada lesson's sentence is \`${await text("lesson")}\``);
  expect((await text("verdict")) === "Input 0 should be rejected, and your program accepted it.", `the nada lesson as it opens says \`${await text("verdict")}\``);
  await click('[data-slot="0"][data-op="nada"]');
  expect((await text("verdict")).startsWith("Solved!"), `after nada on line 1 the nada lesson says \`${await text("verdict")}\``);
  expect(await evaluate(`document.querySelector('[data-puzzle="${nadaAt}"]').classList.contains("solved")`), "the nada lesson isn't marked solved by its pick");
  expect(!(await evaluate(`document.querySelector('[data-puzzle="${nadaAt + 1}"]').disabled`)), "solving the nada lesson doesn't open the next puzzle");
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
console.log(`ok   page_check: the wasm matches the C on all ${PROGRAMS * 4} runs and its ${puzzles.length}-puzzle table (${first.name} ${first.count} programs, shortest ${first.shortest}; ... ${hardest.name} ${hardest.count}, shortest ${hardest.shortest}); in Chromium at ${WIDTH}px with no sideways scroll the first puzzle was solved by clicks, the second (an AX puzzle) opened, the progress outlived a reload, an old save kept its solved languages, and the three AX lessons and the nada lesson were each solved by one click (${checks} checks)`);
