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
// - counts, from the wasm alone, the programs that accept each language
//   and the length of the shortest (its ops before its trailing `nada`s),
//   and holds the C's puzzle table to them and to its order: the five
//   lessons first, then by shortest length and count; each example fits
//   in its shortest length, as the page's locked lines need;
// - serves <site-dir> on a loopback port and loads it in headless Chromium
//   (Playwright's, found by PLAYWRIGHT_BROWSERS_PATH, which Shale sets) at a
//   phone's width; there it sees the first lesson's sentence and its lines
//   past the shortest locked to nada and no Run button, reads which input
//   the fresh all-nada program gets wrong, then solves it by clicking each
//   slot's opcode (solved at the last pick, not before), reads the trace
//   and the count of programs, sees that neither the
//   page nor the trace scrolls sideways, sees the second puzzle open, and
//   after a reload sees the first still solved. Then it loads an older
//   save (a place, `current`, from the old order) and sees the solved
//   language kept, and the nada lesson open unsolved (`zero` on its one
//   line) and solved by one pick, nada.
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
// The lessons' languages, by code (a member n is bit n): {0}, {1}, {0, 2}, {2}, { }.
const LESSONS = [1, 2, 5, 4, 0];
const files = readdirSync(site);
const wasmFile = files.find((f) => /^nibble\.[0-9a-f]{16}\.wasm$/.test(f));
if (!wasmFile) { console.error(`page_check: no hashed wasm in ${site}`); process.exit(1); }

// ── the wasm against the C ───────────────────────────────────────────────
const { instance } = await WebAssembly.instantiate(readFileSync(join(site, wasmFile)));
const m = instance.exports;
const runs = JSON.parse(readFileSync(runsFile, "utf8"));
const table = JSON.parse(readFileSync(puzzlesFile, "utf8"));
expect(runs.length === PROGRAMS * 4, `runs.json holds ${runs.length} runs`);

const count = new Array(16).fill(0);
const shortest = new Array(16).fill(STEPS + 1);
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
  count[lang]++;
  shortest[lang] = Math.min(shortest[lang], len);
}
expect(mismatches === 0, `${mismatches} of ${PROGRAMS * 4} runs differ between the wasm and the C`);

const puzzles = table.puzzles;
expect(puzzles.length === 16 && new Set(puzzles.map((p) => p.code)).size === 16, "the table has each of the 16 languages once");
for (let i = 0; i < puzzles.length; i++) {
  const p = puzzles[i];
  expect(p.count === count[p.code], `${p.name}: the C counts ${p.count} programs, the wasm ${count[p.code]}`);
  expect(p.shortest === shortest[p.code], `${p.name}: the C says shortest ${p.shortest}, the wasm ${shortest[p.code]}`);
  expect(m.accepts(p.example) === p.code, `${p.name}: its example program ${p.example} accepts another language`);
  // A puzzle opens as a fresh all-nada program: only { } is solved by it.
  expect((m.accepts(0) === p.code) === (p.code === 0), `${p.name}: the all-nada program ${p.code === 0 ? "doesn't solve" : "solves"} it`);
  // The page locks lines past `lines` to nada: a solution must fit. Only
  // the nada lesson has more lines than its shortest program, one.
  expect(p.lines === (p.code === 0 ? 1 : p.shortest), `${p.name}: ${p.lines} choosable lines, shortest ${p.shortest}`);
  expect(Math.floor(p.example / 4 ** p.lines) === 0, `${p.name}: its example ${p.example} has an op past line ${p.lines}`);
  // It opens unsolved, with nothing on a locked line.
  expect(m.accepts(p.start) !== p.code, `${p.name}: its start program ${p.start} solves it`);
  expect(Math.floor(p.start / 4 ** p.lines) === 0, `${p.name}: its start program ${p.start} has an op on a locked line`);
  expect((p.lesson !== undefined) === (i < LESSONS.length), `${p.name}: a lesson sentence only on the first five`);
  if (i < LESSONS.length) expect(p.code === LESSONS[i], `puzzle ${i + 1} is ${p.name}, not lesson ${i + 1}`);
  else if (i > LESSONS.length) {
    const q = puzzles[i - 1];
    expect(q.shortest < p.shortest || (q.shortest === p.shortest && q.count >= p.count), `${p.name} comes after a harder puzzle, ${q.name}`);
  }
}
expect(puzzles.slice(0, 5).map((p) => p.shortest).join(" ") === "1 2 2 3 0", `the lessons' shortest lengths are ${puzzles.slice(0, 5).map((p) => p.shortest).join(" ")}`);
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
  expect((await text("title")) === `Puzzle 1 of 16: ${first.name}`, `the first puzzle's title is \`${await text("title")}\``);
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

  // The second puzzle, by Next, then a reload: the progress kept.
  await click("#next");
  expect((await text("title")) === `Puzzle 2 of 16: ${puzzles[1].name}`, `Next shows \`${await text("title")}\``);
  expect((await locks()) === wantLocks(puzzles[1]), `the second puzzle's lines are \`${await locks()}\``);
  // Past the first puzzle the intro folds away behind Help, which opens and
  // closes it.
  expect((await evaluate("document.getElementById('intro').hidden")) && !(await evaluate("document.getElementById('help').hidden")), "the second puzzle shows the intro, or no Help link");
  await click("#help");
  expect(!(await evaluate("document.getElementById('intro').hidden")) && (await text("help")) === "Hide help", "Help doesn't open the intro");
  await click("#help");
  expect((await evaluate("document.getElementById('intro').hidden")) && (await text("help")) === "Help", "Hide help doesn't close the intro");
  await open(url);
  expect(await evaluate("document.querySelector('[data-puzzle=\"0\"]').classList.contains('solved')"), "after a reload the first puzzle isn't marked solved");
  expect((await text("title")) === `Puzzle 2 of 16: ${puzzles[1].name}`, `after a reload the page shows \`${await text("title")}\``);

  // An older save, from when the order was by count: its puzzle by place
  // (`current`) is dropped, and a solved language ({2}) stays solved and
  // open wherever it now stands, which opens the nada lesson after it. That
  // lesson has one line, which opens as zero, so it opens unsolved, and its
  // locked lines stay nada over a saved draft; one pick, nada, solves it.
  const twoAt = puzzles.findIndex((p) => p.code === 4);
  const nadaAt = puzzles.findIndex((p) => p.code === 0);
  expect(nadaAt === twoAt + 1, "the nada lesson doesn't follow {2}");
  await evaluate(`localStorage.setItem("nibble-progress-v1", JSON.stringify({ solved: { "4": [2, 2, 1, 0, 0, 0] }, current: 3, drafts: { "0": [1, 2, 3, 1, 2, 3] } }))`);
  await open(url);
  expect((await text("title")) === `Puzzle 1 of 16: ${first.name}`, `an old save's place shows \`${await text("title")}\``);
  expect(await evaluate(`(() => { const b = document.querySelector('[data-puzzle="${twoAt}"]'); return b.classList.contains("solved") && !b.disabled; })()`), "an old save's solved {2} isn't solved and open");
  expect(await evaluate(`(() => { const b = document.querySelector('[data-puzzle="${nadaAt}"]'); return !b.classList.contains("solved") && !b.disabled; })()`), "the nada lesson isn't open and unsolved");
  await click(`[data-puzzle="${nadaAt}"]`);
  expect((await text("title")) === `Puzzle ${nadaAt + 1} of 16: always_false`, `the nada lesson's title is \`${await text("title")}\``);
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
console.log(`ok   page_check: the wasm matches the C on all ${PROGRAMS * 4} runs and its ${puzzles.length}-puzzle table (${first.name} ${first.count} programs, shortest ${first.shortest}; ... ${hardest.name} ${hardest.count}, shortest ${hardest.shortest}); in Chromium at ${WIDTH}px with no sideways scroll the first puzzle was solved by clicks, the second opened, the progress outlived a reload and an old save kept its solved language (${checks} checks)`);
