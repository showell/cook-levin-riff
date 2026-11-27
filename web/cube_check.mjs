// cube_check: the rolling cube's page, held to the C.
//
//   node web/cube_check.mjs <site-dir> <cube_runs.txt> <cube_pars4.txt> <census4.txt> <screenshot.png>
//
// web/build.shale runs it on what it built. It:
//
// - loads the page's wasm (cube.<hash>.wasm in <site-dir>) in node and holds
//   it to cube/cube.c on <cube_runs.txt>, web/cube_data.c's 400 programs,
//   frame by frame, and the roll that leaves the board where one does;
// - holds the page's table (cube_pars.<hash>.bin) to the census: its 65,536
//   bytes to the pars in <cube_pars4.txt> (census 4's every pattern), and
//   their count at each par to the table in <census4.txt>, the golden
//   report; and rolls every one of the census's 65,536 solutions through
//   the wasm, each ending with exactly its pattern lit, in its par;
// - serves <site-dir> on a loopback port and loads cube.html in headless
//   Chromium (Playwright's, found by PLAYWRIGHT_BROWSERS_PATH, which Shale
//   sets), and sees the canvas draw the cube with its painted face up; plays
//   the "b2 alone" sample to its end and reads par 8, yours 8 and b2 lit,
//   with lit tiles on the canvas; types programs (and one with a letter that
//   is not a roll, and one off the board); rolls by the arrow keys, one
//   refused at the wall; steps forward and back and resets; sees the "every
//   tile" sample's par 47; and at a phone's width sees nothing scroll
//   sideways and nothing stored. It saves screenshots to <screenshot.png>,
//   and beside it `-rolling` (mid-roll) and `-phone`.
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
    if (next & OFF) { out.push(`off@${i}`); break; }
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
  if (want.length > 0 && want[want.length - 1].startsWith("off@")) offs++;
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
  const ok = program.length === table[p] && !out.some((f) => f.startsWith("off")) && (word & 0xffff) === p;
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
const litCells = () => evaluate(`Array.from(document.querySelectorAll("#lit-map div")).map((d, i) => d.classList.contains("lit") ? d.title : null).filter(Boolean).join(",")`);
async function screenshot(file) {
  await evaluate("window.scrollTo(0, 0)");
  const { data } = await page("Page.captureScreenshot", { format: "png", captureBeyondViewport: true });
  writeFileSync(file, Buffer.from(data, "base64"));
}

try {
  await open(1000, 1100, false);
  expect((await evaluate("document.title")) === "Rolling cube", `the page's title is ${await evaluate("document.title")}`);
  expect((await evaluate("!document.getElementById('loading')")), "the page still says it is loading");
  const start = await pixels();
  expect(start.ivory > 500 && start.paint > 300 && start.amber === 0, `the start's canvas: ${JSON.stringify(start)} (the cube's ivory sides, its paint up, nothing lit)`);
  expect((await text("par")) === "par 0; yours is 0", `the start's readout: ${await text("par")}`);

  await click('#samples button[data-program="ESENSNWS"]');
  expect(await settled(8), "the b2 sample did not play to its 8th frame");
  expect((await text("par")) === "par 8; yours is 8", `b2 alone's readout: ${await text("par")}`);
  expect((await litCells()) === "b2", `b2 alone lights ${await litCells()}`);
  expect((await text("length")) === "8 rolls", `b2 alone's length: ${await text("length")}`);
  // b2's tile is under the cube, which ends on it; the diagonal's three
  // other lit tiles show.
  await click('#samples button[data-program="ESSENNWWESESESNNWW"]');
  expect(await settled(18), "the diagonal sample did not play to its end");
  expect((await text("par")) === "par 18; yours is 18" && (await litCells()) === "a1,b2,c3,d4", `the diagonal: ${await text("par")}, lit ${await litCells()}`);
  const diagonal = await pixels();
  expect(diagonal.amber > 600, `after the diagonal the canvas shows ${diagonal.amber} lit pixels`);
  await screenshot(shotFile);
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
  // refused, a roll off the board stops the run.
  await typeProgram("es e");
  expect(await settled(3), "the typed `es e` did not show its end");
  expect((await text("par")) === "par 3; yours is 3" && (await litCells()) === "c2", `es e: ${await text("par")}, lit ${await litCells()}`);
  await typeProgram("ESX");
  expect((await text("said")).includes("not a roll") && (await text("par")) === "par 3; yours is 3", `ESX: ${await text("said")} / ${await text("par")}`);
  await typeProgram("EN");
  expect((await text("status")).includes("leave the board") && (await text("par")).startsWith("No par"), `EN: ${await text("status")} / ${await text("par")}`);

  // The arrow keys roll the cube; north from a1 is refused.
  await click("#clear");
  await evaluate("document.activeElement.blur()");
  await key("ArrowUp");
  expect((await text("said")).includes("would leave the board") && (await evaluate("document.getElementById('program').value")) === "", `ArrowUp at a1: ${await text("said")}`);
  const arrows = ["ArrowRight", "ArrowDown", "ArrowRight"];
  for (let i = 0; i < arrows.length; i++) {
    await key(arrows[i]);
    expect(await settled(i + 1), `${arrows[i]} did not roll to frame ${i + 1}`);
  }
  expect((await evaluate("document.getElementById('program').value")) === "ESE" && (await litCells()) === "c2", `the arrow keys made ${await evaluate("document.getElementById('program').value")}, lit ${await litCells()}`);

  await click('#samples button[data-program="EEESWNWSENWSWNESWESWESWNESENWNESESWNESWNEWNEWNE"]');
  expect((await text("par")) === "par 47; yours is 47", `every tile's readout: ${await text("par")}`);

  await open(390, 900, true);
  const widths = await evaluate("(() => { const s = document.scrollingElement; return { page: s.scrollWidth, box: s.clientWidth }; })()");
  expect(widths.page <= widths.box, `at 390px the page scrolls sideways: ${JSON.stringify(widths)}`);
  await click('#samples button[data-program="ESENSNWS"]');
  expect(await settled(8), "at 390px the b2 sample did not play to its end");
  expect((await text("par")) === "par 8; yours is 8", `at 390px b2 alone's readout: ${await text("par")}`);
  await screenshot(shotFile.replace(/\.png$/, "-phone.png"));
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

if (failures > 0) { console.error(`cube_check: ${failures} of ${checks} checks failed`); process.exit(1); }
console.log(`ok   cube_check: the wasm matches cube.c on ${runLines.length} programs (${frames} frames, ${offs} off the board) and rolls all ${solutions} census solutions to their patterns; the table's 65,536 pars are the census's; in Chromium the page drew the cube, played b2 alone to par 8, stepped back, forward and reset, took typed programs and the arrow keys, refused a non-roll and the wall, played the diagonal to par 18 with its lit tiles drawn, showed every tile's par 47 and, at 390px with no sideways scroll, played b2 alone again (${checks} checks)`);
