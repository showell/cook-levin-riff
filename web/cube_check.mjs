// cube_check: the rolling cube's page, held to the C.
//
//   node web/cube_check.mjs <site-dir> <cube_runs.txt> <cube_pars4.txt> <census4.txt> <screenshot.png>
//
// web/build.shale runs it on what it built. It:
//
// - loads the page's wasm (cube.<hash>.wasm in <site-dir>) in node and holds
//   it to cube/cube.c on <cube_runs.txt>, web/cube_data.c's 400 programs,
//   frame by frame, and the roll that halts the machine where one does;
//   and its `max_length()` to the census's L;
// - holds the page's table (cube_pars.<hash>.bin) to the census: its 65,536
//   bytes to the pars in <cube_pars4.txt> (census 4's every pattern), and
//   their count at each par to the table in <census4.txt>, the golden
//   report; and rolls every one of the census's 65,536 solutions through
//   the wasm, each ending with exactly its pattern lit, in its par;
// - serves <site-dir> on a loopback port and loads cube.html in headless
//   Chromium (Playwright's, found by PLAYWRIGHT_BROWSERS_PATH, which Shale
//   sets). At 1366 x 650 it sees three panes side by side and nothing
//   scroll, and the canvas draw the cube with its painted face up; plays
//   the "b2 alone" sample to its end and reads par 8, yours 8 and b2 lit,
//   the lit set 32 in decimal and binary, with lit tiles on the canvas;
//   holds the page's ten questions, and the help's table of them, to
//   QUESTIONS below, their pars to the table, and chooses is_mult_of_5 and
//   answers it with the diagonal sample; steps forward and back and resets; types
//   programs (one with a letter that is not a roll, one that halts, one
//   longer than L, refused); rolls by the arrow keys, the button lighting,
//   north from a1 halting the machine; rolls by clicking the tiles next to
//   the cube and by dragging the cube, a click on the cube or the sky doing
//   nothing; plays the halt sample to its halt; refuses a roll past L; sees
//   three panes and no scroll at 1920 x 700 too; at a phone's width one
//   column, nothing sideways and nothing stored; and the help page, its L
//   the census's, its tile numbering and its questions. It saves screenshots to <screenshot.png> (1366 x 650),
//   and beside it `-rolling` (mid-roll), `-halt`, `-wide` (1920 x 700) and
//   `-phone`.
//
// The browser is started as web/page_check.mjs starts it.
import { spawn } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { createServer } from "node:http";
import { tmpdir } from "node:os";
import { extname, join } from "node:path";

const [site, runsFile, parsFile, censusFile, shotFile] = process.argv.slice(2);
if (!shotFile) {
  console.error("usage: node web/cube_check.mjs <site-dir> <cube_runs.txt> <cube_pars4.txt> <census4.txt> <screenshot.png>");
  process.exit(2);
}

let failures = 0;
let checks = 0;
function expect(ok, why) {
  checks++;
  if (!ok) { failures++; console.error(`cube_check: FAIL ${why}`); }
}

const files = readdirSync(site);
const wasmFile = files.find((f) => /^cube\.[0-9a-f]{16}\.wasm$/.test(f));
const binFile = files.find((f) => /^cube_pars\.[0-9a-f]{16}\.bin$/.test(f));
if (!wasmFile || !binFile) { console.error(`cube_check: no hashed cube wasm or pars table in ${site}`); process.exit(1); }

// ── the wasm against the C ────────────────────────────────────────────
const { instance } = await WebAssembly.instantiate(readFileSync(join(site, wasmFile)));
const roll = instance.exports.roll;
const OFF = 1 << 24;
const ROLLS = "NESW";
const frameText = (w) => `${(w >> 16) & 15}.${(w >> 20) & 7}.${w & 0xffff}`;

function framesOf(program) {
  const out = [];
  let word = 0;
  for (let i = 0; i < program.length; i++) {
    const next = roll(word, ROLLS.indexOf(program[i]));
    if (next & OFF) { out.push(`halt@${i}`); break; }
    out.push(frameText(next));
    word = next;
  }
  return { out, word };
}

const runLines = readFileSync(runsFile, "utf8").trim().split("\n");
let frames = 0, offs = 0;
for (const line of runLines) {
  const [program, ...want] = line.split(" ");
  const got = framesOf(program === "-" ? "" : program).out;
  frames += want.length;
  if (want.length > 0 && want[want.length - 1].startsWith("halt@")) offs++;
  expect(got.join(" ") === want.join(" "), `the wasm's run of ${program} is ${got.join(" ")}, cube.c's ${want.join(" ")}`);
}
expect(runLines.length === 400, `${runsFile} has ${runLines.length} programs, not 400`);

// ── the table against the census ──────────────────────────────────────
const table = readFileSync(join(site, binFile));
expect(table.length === 65536, `${binFile} has ${table.length} bytes`);
const parLines = readFileSync(parsFile, "utf8").trim().split("\n");
expect(parLines.length === 65536, `${parsFile} has ${parLines.length} lines`);
let solutions = 0;
parLines.forEach((line, p) => {
  const [pattern, par, sol] = line.split(" ");
  if (Number(pattern) !== p || table[p] !== Number(par)) { expect(false, `pattern ${p}: the table says ${table[p]}, the census ${line}`); return; }
  const program = sol === "-" ? "" : sol;
  const { out, word } = framesOf(program);
  const ok = program.length === table[p] && !out.some((f) => f.startsWith("halt")) && (word & 0xffff) === p;
  if (!ok) expect(false, `the census's solution of pattern ${p}, ${sol}, rolls in the wasm to ${out[out.length - 1]}`);
  else solutions++;
});
expect(solutions === 65536, `${solutions} of 65,536 census solutions roll to their patterns in the wasm`);
const census = readFileSync(censusFile, "utf8");
const histogram = census.split("\npar  patterns\n")[1].split("\n\n")[0].trim().split("\n").map((l) => l.trim().split(/\s+/).map(Number));
const counted = new Map();
for (const b of table) counted.set(b, (counted.get(b) ?? 0) + 1);
for (const [par, count] of histogram) expect((counted.get(par) ?? 0) === count, `${count} patterns have par ${par} in ${censusFile}, ${counted.get(par) ?? 0} in the table`);
expect(Math.max(...table) === 47 && histogram.length === 48, `the table's largest par is ${Math.max(...table)}, the census's ${histogram.length - 1}`);
// L: the census's line, the wasm's max_length(), and the sets decided within it.
const lLine = census.match(/programs of at most L = (\d+) rolls/);
const decided = census.match(/decided within L: (\d+) of 65536/);
const L = lLine ? Number(lLine[1]) : NaN;
expect(lLine && instance.exports.max_length(0) === L, `the wasm's max_length() is ${instance.exports.max_length(0)}, the census's L ${lLine?.[1]}`);
expect(decided && Number(decided[1]) === table.filter((b) => b <= L).length, `the census decides ${decided?.[1]} sets within L, the table ${table.filter((b) => b <= L).length}`);

// The page's questions: each name, the tiles it wants, its number and its
// par, held here so that a change to a rule or to the census shows.
const QUESTIONS = [
  ["is_even", [0, 2, 4, 6, 8, 10, 12, 14], 21845, 24],
  ["is_odd", [1, 3, 5, 7, 9, 11, 13, 15], 43690, 26],
  ["is_small", [0, 1, 2, 3, 4, 5, 6, 7], 255, 23],
  ["is_small_even", [0, 2, 4, 6], 85, 12],
  ["is_mult_of_4", [0, 4, 8, 12], 4369, 12],
  ["is_mult_of_5", [0, 5, 10, 15], 33825, 18],
  ["is_square", [0, 1, 4, 9], 531, 13],
  ["is_prime", [2, 3, 5, 7, 11, 13], 10412, 20],
  ["is_fib", [1, 2, 3, 5, 8, 13], 8494, 20],
  ["is_not_14", [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15], 49151, 44],
];
for (const [name, tiles, number, par] of QUESTIONS) {
  expect(tiles.reduce((s, n) => s + 2 ** n, 0) === number, `${name}'s tiles ${tiles} make ${tiles.reduce((s, n) => s + 2 ** n, 0)}, not ${number}`);
  expect(table[number] === par, `${name}, ${number}, has par ${table[number]} in the census, not ${par}`);
}
const questionRows = QUESTIONS.map(([name, , number, par]) => `${name} ${number} ${par}`).join("; ");

// ── the page in Chromium ──────────────────────────────────────────────
const TYPES = { ".html": "text/html", ".css": "text/css", ".js": "text/javascript", ".wasm": "application/wasm", ".json": "application/json" };
const server = createServer((req, res) => {
  const name = req.url === "/" ? "index.html" : decodeURIComponent(req.url.slice(1));
  if (!files.includes(name)) { res.writeHead(404); res.end(); return; }
  res.writeHead(200, { "content-type": TYPES[extname(name)] ?? "application/octet-stream" });
  res.end(readFileSync(join(site, name)));
});
await new Promise((up) => server.listen(0, "127.0.0.1", up));
const url = `http://127.0.0.1:${server.address().port}/cube.html`;

const sleep = (ms) => new Promise((done) => setTimeout(done, ms));
const BROWSERS = process.env.PLAYWRIGHT_BROWSERS_PATH || `${process.env.HOME}/.cache/ms-playwright`;
const chromium = (existsSync(BROWSERS) ? readdirSync(BROWSERS) : [])
  .filter((n) => /^chromium-\d+$/.test(n) && existsSync(`${BROWSERS}/${n}/chrome-linux/chrome`))
  .sort((a, b) => Number(b.slice(9)) - Number(a.slice(9)))[0];
if (!chromium) { console.error(`cube_check: no Chromium in ${BROWSERS}`); process.exit(2); }

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

async function evaluate(expression) {
  const { result, exceptionDetails } = await page("Runtime.evaluate", { expression, awaitPromise: true, returnByValue: true });
  if (exceptionDetails) throw new Error(exceptionDetails.exception?.description ?? exceptionDetails.text);
  return result.value;
}
async function open(width, height, mobile) {
  await page("Emulation.setDeviceMetricsOverride", { width, height, deviceScaleFactor: 1, mobile });
  const done = new Promise((d) => { loaded = d; });
  await page("Page.navigate", { url });
  await done;
  for (let k = 0; k < 50 && !(await evaluate("!!document.querySelector('#samples button') && document.getElementById('scene').dataset.frame === '0'")); k++) await sleep(100);
}
const text = (id) => evaluate(`document.getElementById(${JSON.stringify(id)})?.textContent ?? null`);
async function click(selector) {
  const box = await evaluate(`(() => { const e = document.querySelector(${JSON.stringify(selector)}); if (!e) return null;
    e.scrollIntoView({ block: "center" }); const r = e.getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2 }; })()`);
  if (!box) throw new Error(`no ${selector} on the page`);
  for (const type of ["mousePressed", "mouseReleased"]) {
    await page("Input.dispatchMouseEvent", { type, x: box.x, y: box.y, button: "left", clickCount: 1 });
  }
  await sleep(30);
}
const KEY_CODES = { ArrowUp: 38, ArrowRight: 39, ArrowDown: 40, ArrowLeft: 37 };
async function key(name) {
  for (const type of ["keyDown", "keyUp"]) {
    await page("Input.dispatchKeyEvent", { type, key: name, code: name, windowsVirtualKeyCode: KEY_CODES[name] });
  }
  await sleep(30);
}
async function typeProgram(value) {
  await evaluate(`(() => { const i = document.getElementById("program"); i.value = ${JSON.stringify(value)}; i.dispatchEvent(new Event("input")); })()`);
}
// Waits for the run to stand still at `frame`.
async function settled(frame) {
  for (let k = 0; k < 200; k++) {
    const s = await evaluate(`(() => { const c = document.getElementById("scene"); return c.dataset.animating + ":" + c.dataset.frame; })()`);
    if (s === `0:${frame}`) return true;
    await sleep(50);
  }
  return false;
}
// The canvas's pixels by kind: the cube's ivory, its blue paint, lit amber.
const pixels = () => evaluate(`(() => {
  const c = document.getElementById("scene"), d = c.getContext("2d").getImageData(0, 0, c.width, c.height).data;
  let ivory = 0, paint = 0, amber = 0;
  for (let i = 0; i < d.length; i += 4) {
    const r = d[i], g = d[i + 1], b = d[i + 2];
    if (r > 170 && g > 165 && b > 155 && Math.abs(r - b) < 40) ivory++;
    if (b > 150 && r < 110 && g < 170) paint++;
    if (r > 220 && g > 140 && g < 215 && b < 150) amber++;
  }
  return { ivory, paint, amber };
})()`);
const litCells = () => evaluate(`Array.from(document.querySelectorAll("#lit-map div")).map((d, i) => d.classList.contains("lit") ? d.dataset.name : null).filter(Boolean).join(",")`);
const number = async () => `${await text("decimal")} ${await text("binary")}`;
async function screenshot(file) {
  await evaluate("window.scrollTo(0, 0)");
  const { data } = await page("Page.captureScreenshot", { format: "png", captureBeyondViewport: true });
  writeFileSync(file, Buffer.from(data, "base64"));
}

// The canvas's point (x, y) in the page, and a press there, a move and a
// release at (x2, y2).
async function pressAt(x, y, x2 = x, y2 = y) {
  const r = await evaluate(`(() => { const b = document.getElementById("scene").getBoundingClientRect(); return { x: b.x, y: b.y }; })()`);
  await page("Input.dispatchMouseEvent", { type: "mouseMoved", x: r.x + x, y: r.y + y });
  await page("Input.dispatchMouseEvent", { type: "mousePressed", x: r.x + x, y: r.y + y, button: "left", clickCount: 1 });
  if (x2 !== x || y2 !== y) {
    for (const k of [0.33, 0.66, 1]) await page("Input.dispatchMouseEvent", { type: "mouseMoved", x: r.x + x + (x2 - x) * k, y: r.y + y + (y2 - y) * k, button: "left" });
  }
  await page("Input.dispatchMouseEvent", { type: "mouseReleased", x: r.x + x2, y: r.y + y2, button: "left", clickCount: 1 });
  await sleep(30);
}
// The rollable neighbours' centres on the canvas, by roll letter.
const targets = async () => Object.fromEntries((await evaluate("document.getElementById('scene').dataset.targets")).split(" ").filter(Boolean)
  .map((t) => { const [r, xy] = t.split(":"); return [r, xy.split(",").map(Number)]; }));
const programValue = () => evaluate("document.getElementById('program').value");
// The page's fit: its scroll against the window, and each pane's box.
const layout = () => evaluate(`(() => {
  const s = document.scrollingElement;
  const panes = Array.from(document.querySelectorAll(".pane")).map((p) => { const r = p.getBoundingClientRect(); return { x: Math.round(r.x), y: Math.round(r.y), w: Math.round(r.width), h: Math.round(r.height), bottom: Math.round(r.bottom) }; });
  const c = document.getElementById("scene").getBoundingClientRect();
  return { scrollW: s.scrollWidth, clientW: s.clientWidth, scrollH: s.scrollHeight, clientH: s.clientHeight, panes, canvas: [Math.round(c.width), Math.round(c.height)] };
})()`);
const measured = [];
function threePanes(l, size) {
  measured.push(`${size}: canvas ${l.canvas[0]} x ${l.canvas[1]}, page ${l.scrollW} x ${l.scrollH}`);
  expect(l.scrollH <= l.clientH && l.scrollW <= l.clientW, `at ${size} the page scrolls: ${JSON.stringify(l)}`);
  const [a, b, c] = l.panes;
  expect(l.panes.length === 3 && a.y === b.y && b.y === c.y && a.x < b.x && b.x < c.x, `at ${size} the panes are not side by side: ${JSON.stringify(l.panes)}`);
  expect(l.panes.every((p) => p.bottom <= l.clientH), `at ${size} a pane ends below the window: ${JSON.stringify(l.panes)}`);
}

try {
  await open(1366, 650, false);
  expect((await evaluate("document.title")) === "Rolling cube", `the page's title is ${await evaluate("document.title")}`);
  expect((await evaluate("!document.getElementById('loading')")), "the page still says it is loading");
  threePanes(await layout(), "1366 x 650");
  const start = await pixels();
  expect(start.ivory > 500 && start.paint > 300 && start.amber === 0, `the start's canvas: ${JSON.stringify(start)} (the cube's ivory sides, its paint up, nothing lit)`);
  expect((await text("par")) === "par 0; yours is 0" && (await text("length")) === `0 of ${L} rolls`, `the start's readout: ${await text("par")} / ${await text("length")}`);
  expect((await number()) === "0 0000 0000 0000 0000", `the start's number: ${await number()}`);
  expect((await evaluate("document.querySelector('label[for=program]').textContent")).includes("RollScript"), "the program's label does not name RollScript");
  expect((await evaluate("Array.from(document.querySelectorAll('#lit-map div')).map((d) => d.textContent + d.dataset.name).join(' ')")) ===
    Array.from({ length: 16 }, (_, n) => `${n}${"abcd"[n % 4]}${Math.floor(n / 4) + 1}`).join(" "), "the lit map's tiles are not numbered 0 to 15 row by row, a1 first");
  const shownQuestions = await evaluate("Array.from(document.querySelectorAll('#questions button')).map((b) => `${b.dataset.question} ${b.dataset.target} ${b.dataset.par}`).join('; ')");
  expect(shownQuestions === questionRows, `the page's questions are ${shownQuestions}, not ${questionRows}`);

  await click('#samples button[data-program="ESENSNWS"]');
  expect(await settled(8), "the b2 sample did not play to its 8th frame");
  expect((await text("par")) === "par 8; yours is 8", `b2 alone's readout: ${await text("par")}`);
  expect((await litCells()) === "b2", `b2 alone lights ${await litCells()}`);
  expect((await text("length")) === `8 of ${L} rolls`, `b2 alone's length: ${await text("length")}`);
  expect((await number()) === "32 0000 0000 0010 0000", `b2 alone's number: ${await number()}`);
  expect((await text("answers")) === "" && (await text("question")) === "", `b2 alone answers ${await text("answers")} / ${await text("question")}`);
  // A question chosen: its four tiles ringed on the map, then answered by
  // the diagonal. b2's tile is under the cube, which ends on it; the
  // diagonal's three other lit tiles show.
  await click('#questions button[data-question="is_mult_of_5"]');
  expect((await text("question")).startsWith("The question is_mult_of_5: light n % 5 == 0, the set 33825, in par 18") &&
    (await evaluate("document.querySelectorAll('#lit-map div.want').length")) === 4, `is_mult_of_5 chosen: ${await text("question")}`);
  await click('#samples button[data-program="ESSENNWWESESESNNWW"]');
  expect(await settled(18), "the diagonal sample did not play to its end");
  expect((await text("par")) === "par 18; yours is 18" && (await litCells()) === "a1,b2,c3,d4", `the diagonal: ${await text("par")}, lit ${await litCells()}`);
  expect((await number()) === "33825 1000 0100 0010 0001", `the diagonal's number: ${await number()}`);
  expect((await text("answers")) === "It answers is_mult_of_5: n % 5 == 0." && (await text("question")) === "Answered is_mult_of_5 in 18 rolls; its par is 18." &&
    (await evaluate("document.querySelector('#questions button[data-question=is_mult_of_5]').classList.contains('answered')")),
    `the diagonal: ${await text("answers")} / ${await text("question")}`);
  const diagonal = await pixels();
  expect(diagonal.amber > 600, `after the diagonal the canvas shows ${diagonal.amber} lit pixels`);
  await screenshot(shotFile);
  await click('#questions button[data-question="is_mult_of_5"]');
  expect((await text("question")) === "" && (await evaluate("document.querySelectorAll('#lit-map div.want').length")) === 0, `is_mult_of_5 let go: ${await text("question")}`);
  await click('#samples button[data-program="ESENSNWS"]');
  expect(await settled(8), "the b2 sample did not play to its 8th frame again");

  // Back, forward and reset, each a frame.
  await click("#back");
  expect(await settled(7), "Back did not step to frame 7");
  await click("#forward");
  await sleep(110);
  await screenshot(shotFile.replace(/\.png$/, "-rolling.png"));
  expect(await settled(8), "Forward did not step to frame 8");
  await click("#reset");
  expect(await settled(0), "Reset did not go back to the start");
  expect((await text("par")) === "par 8; yours is 8", `after Reset the program's readout is ${await text("par")}`);

  // Typed programs: spaces and lower case pass, a letter that is no roll is
  // refused, a roll off the board halts the run, and a program past L is
  // refused.
  await typeProgram("es e");
  expect(await settled(3), "the typed `es e` did not show its end");
  expect((await text("par")) === "par 3; yours is 3" && (await litCells()) === "c2", `es e: ${await text("par")}, lit ${await litCells()}`);
  await typeProgram("ESX");
  expect((await text("said")).includes("not a roll") && (await text("par")) === "par 3; yours is 3", `ESX: ${await text("said")} / ${await text("par")}`);
  await typeProgram("EN");
  expect(await settled(2), "the typed EN did not show its halt, frame 2");
  expect((await text("status")).startsWith("Halted at roll 2") && (await text("par")) === "par 0; yours is 2" && (await evaluate("document.getElementById('scene').dataset.halted")) === "1",
    `EN: ${await text("status")} / ${await text("par")}`);
  await typeProgram("E".repeat(L + 1));
  expect((await text("said")).includes(`at most ${L}`) && (await text("length")) === `2 of ${L} rolls`, `${L + 1} rolls: ${await text("said")} / ${await text("length")}`);

  // The arrow keys roll the cube, lighting their button; north from a1
  // joins the program and halts the machine.
  await click("#clear");
  await evaluate("document.activeElement.blur()");
  await key("ArrowUp");
  expect((await evaluate("document.querySelector('.keys [data-roll=N]').classList.contains('pressed')")), "ArrowUp did not light the N button");
  expect(await settled(1), "ArrowUp at a1 did not show the halt");
  expect((await programValue()) === "N" && (await text("status")).startsWith("Halted at roll 1"), `ArrowUp at a1: ${await programValue()} / ${await text("status")}`);
  await click("#clear");
  await evaluate("document.activeElement.blur()");
  const arrows = ["ArrowRight", "ArrowDown", "ArrowRight"];
  for (let i = 0; i < arrows.length; i++) {
    await key(arrows[i]);
    expect(await settled(i + 1), `${arrows[i]} did not roll to frame ${i + 1}`);
  }
  expect((await programValue()) === "ESE" && (await litCells()) === "c2", `the arrow keys made ${await programValue()}, lit ${await litCells()}`);

  // Clicks on the tiles next to the cube: from a1 only E and S; each click
  // rolls that way. A click on the sky or on the cube does nothing, and a
  // drag from the cube onto a tile rolls there.
  await click("#clear");
  expect(await settled(0), "Clear did not go back to the start");
  const fromA1 = await targets();
  expect(Object.keys(fromA1).sort().join("") === "ES", `from a1 the clickable tiles are ${Object.keys(fromA1).join("")}`);
  await pressAt(4, 4);
  const cubeAt = (await evaluate("document.getElementById('scene').dataset.cube")).split(",").map(Number);
  await pressAt(cubeAt[0], cubeAt[1]);
  expect((await programValue()) === "", `a click on the sky or the cube rolled: ${await programValue()}`);
  let made = "";
  for (const r of ["E", "S", "E", "N", "W", "S", "W", "N"]) {
    const t = await targets();
    if (!t[r]) { expect(false, `after ${made} no tile to click for ${r}: ${JSON.stringify(t)}`); break; }
    await pressAt(...t[r]);
    made += r;
    expect(await settled(made.length) && (await programValue()) === made, `clicking ${r} after ${made.slice(0, -1)} made ${await programValue()}`);
  }
  const before = await programValue();
  const pull = (await evaluate("document.getElementById('scene').dataset.cube")).split(",").map(Number);
  const pullTo = (await targets()).E;
  await pressAt(pull[0], pull[1], pullTo[0], pullTo[1]);
  expect(await settled(before.length + 1) && (await programValue()) === before + "E", `the drag from the cube east made ${await programValue()}`);

  // The halt sample: its fifth roll leaves the board, and the rolls after
  // it do nothing.
  await click('#samples button[data-program="ESENNSSS"]');
  expect(await settled(5), "the halt sample did not play to its halt, frame 5");
  expect((await text("status")).startsWith("Halted at roll 5") && (await litCells()) === "c2" && (await text("par")) === "par 3; yours is 8",
    `the halt sample: ${await text("status")} / ${await litCells()} / ${await text("par")}`);
  expect((await evaluate("document.querySelectorAll('#rolls span.dead').length")) === 3 && (await evaluate("document.querySelectorAll('#rolls span.halt').length")) === 1, "the halt sample's rolls line does not mark the halt and the three dead rolls");
  await screenshot(shotFile.replace(/\.png$/, "-halt.png"));

  // The longest sample is L rolls; one more roll is refused.
  const longest = "EEESWNESWNESSWWENWENWSWNESWESWESWNESNESENWNES";
  expect(longest.length === L, `the longest sample is ${longest.length} rolls, L ${L}`);
  await typeProgram(longest);
  expect(await settled(L), "the longest program did not show its end");
  expect((await text("par")) === `par ${L}; yours is ${L}` && (await text("length")) === `${L} of ${L} rolls`, `the longest: ${await text("par")} / ${await text("length")}`);
  await evaluate("document.activeElement.blur()");
  await key("ArrowLeft");
  expect((await text("said")).includes(`at most ${L}`) && (await programValue()) === longest, `a roll past L: ${await text("said")}`);

  await open(1920, 700, false);
  threePanes(await layout(), "1920 x 700");
  await screenshot(shotFile.replace(/\.png$/, "-wide.png"));

  await open(390, 844, true);
  const phone = await layout();
  measured.push(`390 x 844: canvas ${phone.canvas[0]} x ${phone.canvas[1]}, page ${phone.scrollW} x ${phone.scrollH}`);
  expect(phone.scrollW <= phone.clientW, `at 390px the page scrolls sideways: ${JSON.stringify(phone)}`);
  expect(phone.panes.length === 3 && phone.panes.every((p) => p.x === phone.panes[0].x) && phone.panes[1].y < phone.panes[0].y && phone.panes[0].y < phone.panes[2].y,
    `at 390px the panes are not one column, the canvas first: ${JSON.stringify(phone.panes)}`);
  await click('#samples button[data-program="ESENSNWS"]');
  expect(await settled(8), "at 390px the b2 sample did not play to its end");
  expect((await text("par")) === "par 8; yours is 8", `at 390px b2 alone's readout: ${await text("par")}`);
  await screenshot(shotFile.replace(/\.png$/, "-phone.png"));
  expect((await evaluate("localStorage.length")) === 0, "the page stored something in localStorage");

  // The help page, by the page's link.
  expect((await evaluate("document.getElementById('help').getAttribute('href')")) === "cube_help.html", "the page's Help link is not cube_help.html");
  const helped = new Promise((d) => { loaded = d; });
  await page("Page.navigate", { url: url.replace("cube.html", "cube_help.html") });
  await helped;
  expect((await evaluate("document.title")) === "Rolling cube help", `the help page's title is ${await evaluate("document.title")}`);
  expect((await text("max-length")) === String(L), `the help page says L is ${await text("max-length")}, the census ${L}`);
  expect((await evaluate("document.getElementById('back-to-cube').getAttribute('href')")) === "cube.html", "the help page's link back is not cube.html");
  expect((await evaluate("document.body.textContent")).includes("RollScript"), "the help page does not name RollScript");
  const numbering = await evaluate("Array.from(document.querySelectorAll('#numbering td')).map((d) => d.textContent).join(' ')");
  expect(numbering === Array.from({ length: 16 }, (_, n) => `${n} ${"abcd"[n % 4]}${Math.floor(n / 4) + 1}`).join(" "), `the help's numbering: ${numbering}`);
  const helpQuestions = await evaluate("Array.from(document.querySelectorAll('#question-table tr')).slice(1).map((r) => { const c = r.querySelectorAll('td'); return `${c[0].textContent} ${c[2].textContent} ${c[3].textContent}`; }).join('; ')");
  expect(helpQuestions === questionRows, `the help's questions are ${helpQuestions}, not ${questionRows}`);
  const help = await evaluate("(() => { const s = document.scrollingElement; return { page: s.scrollWidth, box: s.clientWidth }; })()");
  expect(help.page <= help.box, `at 390px the help page scrolls sideways: ${JSON.stringify(help)}`);
  expect(thrown.length === 0, `the page threw: ${thrown.join("; ")}`);
} finally {
  socket.close();
  browser.removeAllListeners("exit");
  browser.kill("SIGKILL");
  server.close();
  await sleep(300);
  rmSync(folder, { recursive: true, force: true });
}

if (failures > 0) { console.error(`cube_check: ${failures} of ${checks} checks failed`); process.exit(1); }
console.log(`ok   cube_check: the wasm matches cube.c on ${runLines.length} programs (${frames} frames, ${offs} halted) and rolls all ${solutions} census solutions to their patterns; the table's 65,536 pars and L = ${L} are the census's; in Chromium the page fit with no scroll (${measured.join("; ")}), played the samples, stepped, took typed programs, the arrow keys and clicks and a drag on the tiles, halted off the board (typed, by key, the halt sample), refused a non-roll and programs past L, numbered its tiles, showed the lit set as a number, chose and answered a question, and its help page says L, the numbering and the ${QUESTIONS.length} questions with their pars (${checks} checks)`);
