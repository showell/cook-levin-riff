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
//   lessons first (is_tiny; is_one and is_even, each with line 1 given, as
//   decr and mod2; grumpy, opening as zero, which nada, decr and mod2 each
//   solve), then the other twelve by shortest length, count and code; each
//   example fits its open lines and its given ones, and each puzzle's start
//   leaves it unsolved;
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
//   each opening the next; and sees a solved puzzle reopen at its start. A
//   second screenshot, of is_one with its given line, goes beside the first
//   (`-given` before `.png`).
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
// The lessons, by code (a member n is bit n), with their open lines, given
// lines and start: {0}; {1} and {0, 2}, line 1 given as decr (2) and mod2
// (3); { }, opening as zero (1).
const LESSONS = [
  { code: 1, name: "is_tiny", lines: 1, given: 0, start: 0 },
  { code: 2, name: "is_one", lines: 2, given: 1, start: 2 },
  { code: 5, name: "is_even", lines: 2, given: 1, start: 3 },
  { code: 0, name: "grumpy", lines: 1, given: 0, start: 1 },
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

const puzzles = table.puzzles;
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
  // The page locks lines past `lines` to nada and the first `given` to the
  // start's ops: the example must fit both.
  expect(Math.floor(p.example / 4 ** p.lines) === 0, `${p.name}: its example ${p.example} has an op past line ${p.lines}`);
  for (let k = 0; k < p.given; k++) expect(opAt(p.example, k) === opAt(p.start, k), `${p.name}: its example's line ${k + 1} isn't the given one`);
  expect(p.given < p.lines, `${p.name}: ${p.given} given of ${p.lines} lines leaves none to pick`);
  // It opens unsolved, with nothing on a locked line.
  expect(!solves(p, p.start), `${p.name}: its start program ${p.start} solves it`);
  expect(Math.floor(p.start / 4 ** p.lines) === 0, `${p.name}: its start program ${p.start} has an op on a locked line`);
  expect((p.lesson !== undefined) === (i < LESSONS.length), `${p.name}: a lesson sentence only on the first four`);
  if (i < LESSONS.length) {
    const l = LESSONS[i];
    const got = `${p.code} ${p.name} ${p.lines} ${p.given} ${p.start}`;
    expect(got === `${l.code} ${l.name} ${l.lines} ${l.given} ${l.start}`, `puzzle ${i + 1} is \`${got}\`, not ${l.name}'s`);
  } else {
    expect(p.lines === p.shortest && p.given === 0 && p.start === 0, `${p.name}: ${p.lines} lines, ${p.given} given, start ${p.start}`);
    if (i > LESSONS.length) {
      const q = puzzles[i - 1];
      const after = q.shortest < p.shortest || (q.shortest === p.shortest && (q.count > p.count || (q.count === p.count && q.code < p.code)));
      expect(after, `${p.name} comes after a harder puzzle, ${q.name}`);
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
// Each line as the page shows it: o (open), L (locked) or G (given: locked,
// its row marked given), then its highlighted op.
const locks = () => evaluate(`[...document.querySelectorAll(".slot")].map((r) => {
  const bs = [...r.querySelectorAll("button")], on = r.querySelector("button.on");
  const kind = bs.every((b) => b.disabled) ? (r.classList.contains("given") ? "G" : "L") : bs.some((b) => b.disabled) ? "?" : "o";
  return kind + (on ? on.dataset.op : "-"); }).join(" ")`);
const wantLocks = (p) => Array.from({ length: STEPS }, (_, k) => (k < p.given ? "G" : k < p.lines ? "o" : "L") + OPS[opAt(p.start, k)]).join(" ");
const solvedNow = async () => (await text("verdict")).startsWith("Solved!");
try {
  await open(url);
  expect((await text("title")) === `Puzzle 1 of 16: ${first.name}`, `the first puzzle's title is \`${await text("title")}\``);
  expect((await text("hint")) === "This can be solved with a one-line Nibble program, so lines 2 to 6 are locked to nada.", `the first puzzle's hint says \`${await text("hint")}\``);
  expect((await evaluate("document.querySelector('[data-slot=\"0\"][data-op=\"zero\"]').title")) === table.ops[1].comment, "zero's tooltip isn't its comment");
  expect(await isShut(1), "the second puzzle is open before the first is solved");
  expect((await text("lesson")) === first.lesson, `the first puzzle's lesson says \`${await text("lesson")}\``);
  expect((await locks()) === wantLocks(first), `the first puzzle's lines are \`${await locks()}\`, not \`${wantLocks(first)}\``);
  expect((await text("intro")).includes("The first four puzzles teach you Nibble"), "the intro doesn't say the first four puzzles teach Nibble");

  // No Run button: the fresh all-nada program has run already, and is wrong.
  expect(!(await evaluate("!!document.getElementById('run')")), "the page has a Run button");
  expect((await text("verdict")) === "Your program falsely rejects 0.", `the all-nada verdict says \`${await text("verdict")}\``);
  expect((await evaluate("document.querySelectorAll('#trace tbody tr').length")) === STEPS + 2, "the trace hasn't a row a step, then result and want");
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

  // Solved again, then the given-line lessons, each by one click on line
  // 2's zero, line 1 given and locked.
  await click('[data-slot="0"][data-op="zero"]');
  expect(await solvedNow(), "zero on line 1 doesn't solve the first puzzle");
  const given = [[1, "decr", "Your program falsely rejects 1."],
    [2, "mod2", "Your program falsely rejects 0 and 2."]];
  for (const [i, op, opening] of given) {
    await click("#next");
    const p = puzzles[i];
    expect((await text("title")) === `Puzzle ${i + 1} of 16: ${p.name}`, `Next shows \`${await text("title")}\`, not ${p.name}`);
    expect((await locks()) === wantLocks(p), `${p.name}'s lines are \`${await locks()}\`, not \`${wantLocks(p)}\``);
    expect((await locks()).startsWith(`G${op} onada L`), `${p.name}'s line 1 isn't given as ${op}`);
    expect((await evaluate(`document.querySelector('[data-slot="0"][data-op="${op}"]').title`)) === `Given: line 1 is ${op}.`, `${p.name}'s given line's title isn't \`Given: line 1 is ${op}.\``);
    expect((await text("hint")) === `Line 1 is already fixed as ${op}. Your job: fix line 2. Lines 3 to 6 are locked to nada.`, `${p.name}'s hint says \`${await text("hint")}\``);
    // The given op is shown light, as the locked nadas are: the locked rows' opacity.
    const shade = await evaluate(`[getComputedStyle(document.querySelector('[data-slot="0"][data-op="${op}"]')).opacity, getComputedStyle(document.querySelector('[data-slot="3"][data-op="nada"]')).opacity].join(" ")`);
    expect(shade.split(" ")[0] === shade.split(" ")[1] && Number(shade.split(" ")[0]) < 1, `${p.name}'s given ${op} isn't light like the locked nadas: opacities ${shade}`);
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
  for (const op of ["decr", "mod2"]) {
    await click(`[data-slot="0"][data-op="${op}"]`);
    expect(await solvedNow(), `${op} on line 1 doesn't solve grumpy`);
  }
  await click('[data-slot="0"][data-op="zero"]');
  expect(!(await solvedNow()), "zero on line 1 solves grumpy");
  await click("#next");
  expect((await text("title")) === `Puzzle 5 of 16: ${puzzles[4].name}`, `after grumpy Next shows \`${await text("title")}\``);
  // Wrong both ways, every input named: zero on is_odd's line 1 accepts 0
  // and rejects 1 and 3.
  await click('[data-slot="0"][data-op="zero"]');
  expect((await text("verdict")) === "Your program falsely accepts 0. It falsely rejects 1 and 3.", `zero on is_odd says \`${await text("verdict")}\``);

  // A solved puzzle reopens as a puzzle: unsolved, at its start, still
  // marked solved in the row.
  await click('[data-puzzle="1"]');
  expect(!(await solvedNow()) && (await text("found")) === "" && (await locks()) === wantLocks(puzzles[1]), `the solved is_one reopens solved: \`${await text("verdict")}\``);
  expect(await isSolvedMark(1), "reopening is_one unmarks it");
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
console.log(`ok   page_check: the wasm matches the C on all ${PROGRAMS * 4} runs and its ${puzzles.length}-puzzle table (${first.name} ${first.count} programs, shortest ${first.shortest}; ... ${hardest.name} ${hardest.count}, shortest ${hardest.shortest}); in Chromium at ${WIDTH}px with no sideways scroll the first puzzle was solved by clicks, a reload started fresh with nothing stored, is_one and is_even were solved by line 2's zero and grumpy by one click (${checks} checks)`);
