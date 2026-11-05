// The playground page. The interpreter runs in worker.js; this file is the
// editor, the buttons, and the Output, Bytecode and Debugger tabs.

import {
  EditorState, EditorView, StateField, StateEffect, Decoration, keymap,
  lineNumbers, highlightActiveLine, highlightActiveLineGutter, drawSelection,
  bracketMatching, indentOnInput, indentUnit, defaultKeymap,
  // Renamed: plain `history` would hide the browser's window.history.
  history as undoHistory, historyKeymap, indentWithTab, setDiagnostics, lintGutter, closeBrackets,
  closeBracketsKeymap, searchKeymap, highlightSelectionMatches,
} from "./vendor/codemirror.js";
import { lox, loxHighlighting } from "./lox.js";
import { encodeSource, decodeSource } from "./share.js";

const $ = (id) => document.getElementById(id);
const DEBUG_STEP_LIMIT = 20000;
const SAVE_KEY = "clox-playground-source";
const THEME_KEY = "clox-playground-theme";

// localStorage can be missing or throw (private windows, blocked storage).
// The page works without it; it just doesn't remember anything.
const storage = {
  get(key) { try { return localStorage.getItem(key); } catch { return null; } },
  set(key, value) { try { localStorage.setItem(key, value); } catch { /* ignore */ } },
};

// =========================================================================
// Editor
// =========================================================================

// Line highlights: the line under the mouse in the Bytecode tab ("hover"),
// and the line of the debugger's current step ("debug").
const setHoverLine = StateEffect.define();
const setDebugLine = StateEffect.define();

const lineHighlights = StateField.define({
  create: () => ({ hover: null, debug: null }),
  update(value, tr) {
    for (const effect of tr.effects) {
      if (effect.is(setHoverLine)) value = { ...value, hover: effect.value };
      if (effect.is(setDebugLine)) value = { ...value, debug: effect.value };
    }
    return value;
  },
});

// The highlights as line decorations, recomputed when they or the text change.
const lineHighlightsPlugin = EditorView.decorations.compute([lineHighlights, "doc"], (state) => {
  const value = state.field(lineHighlights);
  const doc = state.doc;
  const ranges = [];
  for (const [line, cls] of [[value.hover, "cm-hover-line"], [value.debug, "cm-debug-line"]]
    .filter(([l]) => l && l <= doc.lines).sort((a, b) => a[0] - b[0])) {
    ranges.push(Decoration.line({ class: cls }).range(doc.line(line).from));
  }
  return Decoration.set(ranges, true);
});

const editorTheme = EditorView.theme({
  "&": { height: "100%", color: "var(--text)", backgroundColor: "var(--code-bg)" },
  ".cm-scroller": { fontFamily: "var(--mono)", fontSize: "13px", lineHeight: "1.65" },
  ".cm-content": { caretColor: "var(--text)", padding: "10px 0" },
  "&.cm-focused .cm-cursor": { borderLeftColor: "var(--text)" },
  ".cm-gutters": { backgroundColor: "var(--code-bg)", color: "var(--gutter)", border: "none" },
  ".cm-activeLine": { backgroundColor: "var(--active-line)" },
  ".cm-activeLineGutter": { backgroundColor: "var(--active-line)", color: "var(--text)" },
  "&.cm-focused .cm-selectionBackground, .cm-selectionBackground, ::selection": {
    backgroundColor: "var(--selection) !important",
  },
  ".cm-hover-line": { backgroundColor: "var(--hover-line)" },
  ".cm-debug-line": { backgroundColor: "var(--debug-line)", boxShadow: "inset 3px 0 var(--teal)" },
  ".cm-tooltip": {
    backgroundColor: "var(--panel)", color: "var(--text)", border: "1px solid var(--border)",
  },
  ".cm-diagnostic-error": { borderLeftColor: "var(--error)" },
  ".cm-lintRange-error": {
    backgroundImage: "none", textDecoration: "underline wavy var(--error)",
    textDecorationSkipInk: "none", textUnderlineOffset: "3px",
  },
  ".cm-matchingBracket": { backgroundColor: "var(--hover-line)", outline: "1px solid var(--border)" },
});

let view;

function createEditor(doc) {
  view = new EditorView({
    parent: $("editor"),
    state: EditorState.create({
      doc,
      extensions: [
        // First, so these win over the default keymap's Mod-Enter.
        keymap.of([
          { key: "Mod-Enter", run: () => { run(); return true; } },
          { key: "Shift-Mod-Enter", run: () => { debug(); return true; } },
        ]),
        lineNumbers(),
        highlightActiveLineGutter(),
        highlightActiveLine(),
        drawSelection(),
        undoHistory(),
        bracketMatching(),
        closeBrackets(),
        indentOnInput(),
        indentUnit.of("  "),
        highlightSelectionMatches(),
        lox,
        loxHighlighting,
        lintGutter(),
        lineHighlightsPlugin,
        lineHighlights,
        editorTheme,
        keymap.of([...closeBracketsKeymap, ...defaultKeymap, ...historyKeymap,
                   ...searchKeymap, indentWithTab]),
        EditorView.updateListener.of((update) => {
          if (update.docChanged) onSourceChanged();
        }),
        EditorView.contentAttributes.of({ "aria-label": "Lox source code" }),
      ],
    }),
  });

  // Hovering a line of code highlights its instructions in the Bytecode tab.
  view.contentDOM.addEventListener("mousemove", (event) => {
    const pos = view.posAtCoords({ x: event.clientX, y: event.clientY });
    highlightBytecodeLine(pos == null ? null : view.state.doc.lineAt(pos).number);
  });
  view.contentDOM.addEventListener("mouseleave", () => highlightBytecodeLine(null));
}

function getCode() {
  return view.state.doc.toString();
}

function setCode(text) {
  view.dispatch({ changes: { from: 0, to: view.state.doc.length, insert: text } });
}

function selectLine(number) {
  const doc = view.state.doc;
  if (number < 1 || number > doc.lines) return;
  const line = doc.line(number);
  view.dispatch({ selection: { anchor: line.from, head: line.to }, scrollIntoView: true });
  view.focus();
}

// =========================================================================
// Workers
// =========================================================================

// Two workers: `runner` runs and debugs programs and is the one Stop throws
// away; `helper` only compiles for the Bytecode tab, so that tab keeps
// working while a long program runs.
let runner = null;
let helper = null;
let runnerReady = false;
let running = null;          // {id, mode} while a program runs
let nextId = 1;

function startWorker(onMessage) {
  const worker = new Worker("worker.js");
  worker.onmessage = (event) => onMessage(event.data);
  worker.onerror = (event) => {
    setStatus(`Worker error: ${event.message}`, "error");
    running = null;
    updateButtons();
  };
  return worker;
}

function startRunner() {
  runnerReady = false;
  runner = startWorker(handleRunnerMessage);
}

function handleRunnerMessage(message) {
  if (message.type === "ready") {
    runnerReady = true;
    if (!running) setStatus("Ready", "");
    updateButtons();
    return;
  }
  if (!running || message.id !== running.id) return;   // from an older run

  switch (message.type) {
    case "output":
      for (const [stream, text] of message.chunks) {
        appendLine(stream, text);
        if (running.mode === "debug" && stream === "out") debugOutput.push(text);
      }
      break;
    case "recording":
      loadRecording(message.recording, message.full, running.source);
      break;
    case "done":
      finishRun(message);
      break;
    case "crash":
      appendLine("err", message.message);
      setStatus("Crashed", "error");
      running = null;
      updateButtons();
      break;
  }
}

function run() {
  startProgram("run");
}

function debug() {
  startProgram("debug");
}

let debugOutput = [];

function startProgram(mode) {
  if (!runnerReady || running) return;
  const source = getCode();
  clearOutput();
  clearDiagnostics();
  $("stats").textContent = "";
  running = { id: nextId++, mode, source };
  dismissHint();
  setPlaying(false);
  if (mode === "debug") {
    debugOutput = [];
    requestBytecode(source);         // the debugger shows the instructions
    showTab("debugger");
    setStatus("Recording…", "running");
  } else {
    showTab("output");
    setStatus("Running…", "running");
  }
  updateButtons();
  runner.postMessage({ type: mode, id: running.id, source, limit: DEBUG_STEP_LIMIT });
}

function finishRun(message) {
  const source = running.source;
  running = null;
  const ms = message.ms < 10 ? message.ms.toFixed(1) : Math.round(message.ms);
  if (message.full) {
    setStatus(`Recorded ${DEBUG_STEP_LIMIT.toLocaleString()} steps (limit)`, "error");
  } else if (message.exitCode === 0) {
    setStatus(message.recorded ? `Recorded in ${ms} ms` : `Finished in ${ms} ms`, "ok");
  } else if (message.exitCode === 65) {
    setStatus("Compile error (exit code 65)", "error");
  } else {
    setStatus("Runtime error (exit code 70)", "error");
  }
  showStats(message, ms);
  if (message.exitCode !== 0 && !message.full) showErrorsInEditor(source);
  updateButtons();
}

// A busy worker can't be interrupted, only thrown away: terminate it and
// start a new one for the next run.
function stop() {
  if (!running) return;
  runner.terminate();
  running = null;
  appendLine("err", "[stopped]");
  setStatus("Stopped", "error");
  startRunner();
  updateButtons();
}

function updateButtons() {
  $("run").disabled = !runnerReady || !!running;
  $("debug").disabled = !runnerReady || !!running;
  $("stop").disabled = !running;
}

function setStatus(text, kind) {
  $("status").textContent = text;
  $("status").dataset.kind = kind;
}

// =========================================================================
// Output, errors in the editor, stats
// =========================================================================

const output = $("output");
let queue = [];
let scheduled = false;
let errorLines = [];   // stderr lines of the current run, for the editor

// Lines arrive in batches; render them once per frame, not once per line.
function appendLine(stream, text) {
  if (stream === "err") errorLines.push(text);
  queue.push([stream, text]);
  if (!scheduled) {
    scheduled = true;
    requestAnimationFrame(renderQueue);
  }
}

function renderQueue() {
  scheduled = false;
  const atBottom = output.scrollTop + output.clientHeight >= output.scrollHeight - 4;
  const fragment = document.createDocumentFragment();
  const animate = queue.length < 40;
  for (const [stream, text] of queue) {
    const line = document.createElement("div");
    line.className = (stream === "err" ? "line err" : "line") + (animate ? " fresh" : "");
    if (stream === "err") addLineLinks(line, text);
    else line.textContent = text;
    fragment.appendChild(line);
  }
  queue = [];
  output.appendChild(fragment);
  if (atBottom) output.scrollTop = output.scrollHeight;
}

// Turn every "[line N]" in an error into a link that selects line N.
function addLineLinks(element, text) {
  let last = 0;
  for (const match of text.matchAll(/\[line (\d+)\]/g)) {
    element.append(text.slice(last, match.index));
    const link = document.createElement("a");
    link.href = "#";
    link.textContent = match[0];
    link.addEventListener("click", (event) => {
      event.preventDefault();
      selectLine(Number(match[1]));
    });
    element.append(link);
    last = match.index + match[0].length;
  }
  element.append(text.slice(last));
}

function clearOutput() {
  queue = [];
  errorLines = [];
  output.textContent = "";
}

// clox reports errors as text; turn them into editor diagnostics:
//   compile: "[line 3] Error at 'x': Expect ';' after value."   (one per error)
//   runtime: "Undefined variable 'x'." then "[line 3] in foo()", ... "[line 9] in script"
function showErrorsInEditor(source) {
  if (source !== getCode()) return;   // edited since; the lines may not match
  const doc = view.state.doc;
  const diagnostics = [];
  // The code part of a line: without its indentation or a trailing comment.
  const lineRange = (n) => {
    const line = doc.line(Math.min(Math.max(n, 1), doc.lines));
    const indent = line.text.length - line.text.trimStart().length;
    let end = line.text.length;
    let inString = false;
    for (let i = 0; i < line.text.length; i++) {
      if (line.text[i] === '"') inString = !inString;
      if (!inString && line.text.startsWith("//", i)) { end = i; break; }
    }
    end = line.text.slice(0, end).trimEnd().length;
    return { from: line.from + indent, to: line.from + Math.max(end, indent) };
  };

  for (const text of errorLines) {
    const m = text.match(/^\[line (\d+)\] Error( at end| at '(.*)')?: (.*)$/);
    if (!m) continue;
    let range = lineRange(Number(m[1]));
    if (m[2] === " at end") {
      range = { from: Math.max(doc.length - 1, 0), to: doc.length };
    } else if (m[3] !== undefined) {
      const line = doc.line(Math.min(Number(m[1]), doc.lines));
      const at = line.text.indexOf(m[3]);
      if (at >= 0 && m[3].length > 0) {
        range = { from: line.from + at, to: line.from + at + m[3].length };
      }
    }
    diagnostics.push({ ...range, severity: "error", message: m[4], source: "compile error" });
  }

  if (diagnostics.length === 0 && errorLines.length >= 2) {
    const where = errorLines[1].match(/^\[line (\d+)\] in (.*)$/);
    if (where) {
      diagnostics.push({
        ...lineRange(Number(where[1])), severity: "error",
        message: `${errorLines[0]} (in ${where[2]})`, source: "runtime error",
      });
    }
  }
  view.dispatch(setDiagnostics(view.state, diagnostics));
}

function clearDiagnostics() {
  view.dispatch(setDiagnostics(view.state, []));
}

function formatBytes(n) {
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / 1024 / 1024).toFixed(1)} MB`;
}

function showStats(message, ms) {
  const s = message.stats;
  if (!s) return;
  const items = [
    ["time", `${ms} ms`],
    ["instructions", s.instructions.toLocaleString()],
    ["GC collections", s.collections.toLocaleString()],
    ["peak heap", formatBytes(s.peakBytes)],
  ];
  const stats = $("stats");
  stats.textContent = "";
  for (const [label, value] of items) {
    const item = document.createElement("span");
    item.className = "stat";
    const b = document.createElement("b");
    b.textContent = value;
    item.append(b, " " + label);
    stats.append(item);
  }
}

// =========================================================================
// Bytecode tab
// =========================================================================

let bytecode = null;              // {source, functions, errors, exitCode}
const bytecodeRequests = new Map(); // request id -> the source it compiles
let bytecodeTimer = null;

function startHelper() {
  helper = startWorker((message) => {
    if (message.type !== "bytecode" || !bytecodeRequests.has(message.id)) return;
    const source = bytecodeRequests.get(message.id);
    bytecodeRequests.delete(message.id);
    bytecode = { ...message, source };
    renderBytecode();
    if (recording && recording.source === bytecode.source) renderStep();
  });
}

function requestBytecode(source) {
  if (bytecode && bytecode.source === source) return;
  if ([...bytecodeRequests.values()].includes(source)) return;   // on its way
  const id = nextId++;
  bytecodeRequests.set(id, source);
  helper.postMessage({ type: "disassemble", id, source });
}

function renderBytecode() {
  const box = $("bytecode");
  box.textContent = "";
  // Dimmed until it matches the code in the editor again (see onSourceChanged).
  box.classList.toggle("stale", bytecode.source !== getCode());
  if (bytecode.exitCode !== 0) {
    const pre = document.createElement("pre");
    pre.className = "line err";
    pre.textContent = bytecode.errors.join("\n") || "Compile error.";
    box.append(pre);
    return;
  }
  bytecode.functions.forEach((fn, index) => {
    const section = document.createElement("section");
    section.className = "bc-function";
    const h = document.createElement("h3");
    h.textContent = fn.name === "<script>" ? "<script>  (top-level code)" : `fun ${fn.name}`;
    section.append(h);
    for (const op of fn.ops) {
      const row = document.createElement("pre");
      row.className = "bc-op";
      row.textContent = op.text;
      row.dataset.line = op.line;
      row.dataset.fn = index;
      row.dataset.offset = op.offset;
      // Hovering an instruction highlights its line in the editor.
      row.addEventListener("mouseenter", () => {
        view.dispatch({ effects: setHoverLine.of(op.line) });
        highlightBytecodeLine(op.line);
      });
      row.addEventListener("mouseleave", () => {
        view.dispatch({ effects: setHoverLine.of(null) });
        highlightBytecodeLine(null);
      });
      section.append(row);
    }
    box.append(section);
  });
}

function highlightBytecodeLine(line) {
  for (const row of document.querySelectorAll(".bc-op.linked")) row.classList.remove("linked");
  if (line == null) return;
  for (const row of document.querySelectorAll(`.bc-op[data-line="${line}"]`)) {
    row.classList.add("linked");
  }
}

function scheduleBytecode() {
  if ($("bytecode-panel").hidden) return;
  clearTimeout(bytecodeTimer);
  bytecodeTimer = setTimeout(() => requestBytecode(getCode()), 300);
}

// =========================================================================
// Debugger
// =========================================================================

let recording = null;   // {steps, functions, full, source, output}

function loadRecording(data, full, source) {
  recording = { ...data, full, source, output: debugOutput };
  lastRendered = null;   // a new recording: nothing to animate from
  $("dbg-empty").hidden = true;
  $("dbg").hidden = false;
  const banner = $("dbg-banner");
  if (full) {
    banner.hidden = false;
    banner.textContent =
      `Recording stopped after ${DEBUG_STEP_LIMIT.toLocaleString()} steps, and the program was ` +
      `halted there. The debugger is for small programs: try fewer loop iterations.`;
  } else {
    banner.hidden = true;
  }
  const slider = $("dbg-slider");
  slider.max = Math.max(recording.steps.length - 1, 0);
  slider.value = 0;
  renderStep();
}

function currentStep() {
  return Number($("dbg-slider").value);
}

function goToStep(n) {
  if (!recording || recording.steps.length === 0) return;
  $("dbg-slider").value = Math.min(Math.max(n, 0), recording.steps.length - 1);
  renderStep();
}

function renderStep() {
  if (!recording) return;
  const steps = recording.steps;
  if (steps.length === 0) {
    $("dbg-position").textContent = "No steps recorded: the program didn't compile.";
    for (const id of ["dbg-code", "dbg-stack", "dbg-frames", "dbg-output"]) $(id).textContent = "";
    return;
  }
  const i = currentStep();
  const step = steps[i];
  const fnName = recording.functions[step.f] ?? "?";
  $("dbg-position").textContent =
    `Step ${(i + 1).toLocaleString()} of ${steps.length.toLocaleString()}` +
    ` · line ${step.l} · ${fnName === "<script>" ? "top-level code" : fnName + "()"}`;
  $("dbg-fn").textContent = `in ${fnName}`;

  // The instructions of the running function, with the next one marked.
  const code = $("dbg-code");
  code.textContent = "";
  const fn = bytecode && bytecode.source === recording.source && bytecode.functions[step.f];
  if (fn) {
    let current = null;
    for (const op of fn.ops) {
      const row = document.createElement("pre");
      row.className = "bc-op" + (op.offset === step.o ? " current" : "");
      row.textContent = op.text;
      code.append(row);
      if (op.offset === step.o) current = row;
    }
    if (current) current.scrollIntoView({ block: "nearest" });
  } else {
    code.textContent = "Loading bytecode…";
  }
  // ...and in the Bytecode tab, if it shows the same program.
  for (const row of document.querySelectorAll("#bytecode .bc-op.current")) row.classList.remove("current");
  const tabRow = document.querySelector(`#bytecode .bc-op[data-fn="${step.f}"][data-offset="${step.o}"]`);
  if (tabRow && bytecode.source === recording.source) tabRow.classList.add("current");

  // Animate only between neighbouring steps; a jump redraws without motion.
  const prev = lastRendered && lastRendered.source === recording.source &&
    Math.abs(lastRendered.index - i) === 1 ? lastRendered : null;
  renderStack(step, prev);
  renderFrames(step, prev);
  lastRendered = {
    index: i, source: recording.source, frameCount: step.c.length,
    stack: step.s.map((value, k) => ({ slot: step.sb + k, value })),
  };

  $("dbg-output").textContent = recording.output.slice(0, step.p).join("\n");

  // The source line, in the editor (only if the code is still the same).
  if (recording.source === getCode()) {
    const line = view.state.doc.line(Math.min(step.l, view.state.doc.lines));
    view.dispatch({
      effects: [setDebugLine.of(step.l), EditorView.scrollIntoView(line.from, { y: "nearest" })],
    });
  }
}

let lastRendered = null;   // the step drawn last, to animate the difference

// The value stack, bottom to top in the DOM (the CSS draws the top first).
// Compared with the previous step, values that are new slide in ("pushed"),
// and values that are gone fade out as "ghost" rows that are then removed.
function renderStack(step, prev) {
  const frameStarts = new Map(step.c.map(([f, , base]) => [base, recording.functions[f]]));
  const now = step.s.map((value, k) => ({ slot: step.sb + k, value }));
  let same = 0;
  if (prev) {
    while (same < now.length && same < prev.stack.length &&
           now[same].slot === prev.stack[same].slot && now[same].value === prev.stack[same].value) {
      same++;
    }
  }

  const stack = $("dbg-stack");
  stack.textContent = "";
  const row = ({ slot, value }, valueClass) => {
    const li = document.createElement("li");
    if (frameStarts.has(slot)) {
      li.classList.add("frame-start");
      const name = frameStarts.get(slot);
      li.dataset.frame = name === "<script>" ? "script" : `${name}()`;
    }
    const index = document.createElement("span");
    index.className = "slot";
    index.textContent = slot;
    const text = document.createElement("span");
    text.className = valueClass;
    text.textContent = value;
    li.append(index, text);
    return li;
  };
  if (step.sb > 0) {
    const note = document.createElement("li");
    note.className = "note";
    note.textContent = `… ${step.sb} lower slots not recorded`;
    stack.append(note);
  }
  now.forEach((item, k) => {
    const li = row(item, "value");
    if (prev && k >= same) li.classList.add("pushed");
    if (k === now.length - 1) li.classList.add("top");
    stack.append(li);
  });
  if (prev) {
    for (const item of prev.stack.slice(same)) {
      const ghost = row(item, "ghost-value");   // not ".value": it's gone already
      ghost.classList.add("ghost");
      ghost.setAttribute("aria-hidden", "true");
      stack.append(ghost);
      setTimeout(() => ghost.remove(), 260);
    }
  }
  if (now.length === 0) {
    const empty = document.createElement("li");
    empty.className = "note";
    empty.textContent = "empty";
    stack.append(empty);
  }
}

// Call frames, innermost first, one card each. Entering a function slides
// its card in.
function renderFrames(step, prev) {
  const frames = $("dbg-frames");
  frames.textContent = "";
  [...step.c].reverse().forEach(([f, line], k) => {
    const li = document.createElement("li");
    li.className = "frame-card" + (k === 0 ? " active" : "");
    li.dataset.depth = step.c.length - k;
    if (k === 0 && prev && step.c.length > prev.frameCount) li.classList.add("entered");
    const name = recording.functions[f];
    const fn = document.createElement("span");
    fn.className = "frame-fn";
    fn.textContent = name === "<script>" ? "script" : `${name}()`;
    const where = document.createElement("span");
    where.className = "frame-line";
    where.textContent = `line ${line}`;
    li.append(fn, "  ", where);
    frames.append(li);
  });
}

function clearDebugLine() {
  view.dispatch({ effects: setDebugLine.of(null) });
}

// Play steps through the recording on its own, a few steps a second.
let playTimer = null;
function setPlaying(on) {
  clearInterval(playTimer);
  playTimer = null;
  if (on && recording && recording.steps.length > 1) {
    if (currentStep() >= recording.steps.length - 1) goToStep(0);
    playTimer = setInterval(() => {
      if (currentStep() >= recording.steps.length - 1) setPlaying(false);
      else goToStep(currentStep() + 1);
    }, 260);
  }
  const button = $("dbg-play");
  button.querySelector("use").setAttribute("href", playTimer ? "#i-pause" : "#i-play");
  button.title = playTimer ? "Pause" : "Play";
  button.setAttribute("aria-label", button.title);
}

const manual = (fn) => () => { setPlaying(false); fn(); };
$("dbg-slider").addEventListener("input", manual(renderStep));
$("dbg-first").addEventListener("click", manual(() => goToStep(0)));
$("dbg-back").addEventListener("click", manual(() => goToStep(currentStep() - 1)));
$("dbg-forward").addEventListener("click", manual(() => goToStep(currentStep() + 1)));
$("dbg-last").addEventListener("click", manual(() => goToStep(Infinity)));
$("dbg-play").addEventListener("click", () => setPlaying(!playTimer));
$("dbg-start").addEventListener("click", () => debug());
// ← and → step while the debugger has focus (the slider handles them itself).
$("debugger-panel").addEventListener("keydown", (event) => {
  if (event.target.id === "dbg-slider") return;
  if (event.key === "ArrowLeft") { event.preventDefault(); manual(() => goToStep(currentStep() - 1))(); }
  if (event.key === "ArrowRight") { event.preventDefault(); manual(() => goToStep(currentStep() + 1))(); }
});

// =========================================================================
// Tabs, sharing, autosave, theme, shortcuts
// =========================================================================

// "output" is a panel of its own on wide screens; "bytecode" and "debugger"
// are the tabs of the Under the hood panel. On phones only one panel shows at
// a time (body[data-pane]), so showing a tab also switches to its panel.
const HOOD_TABS = ["bytecode", "debugger"];

function showTab(name, { switchPane = true } = {}) {
  if (name === "output") {
    if (switchPane) setPane("output");
    return;
  }
  for (const tab of HOOD_TABS) {
    const selected = tab === name;
    $(`tab-${tab}`).setAttribute("aria-selected", String(selected));
    $(`${tab}-panel`).hidden = !selected;
  }
  $("hood-hint").textContent = name === "bytecode"
    ? "Hover the code or an instruction to link them."
    : "Click a control, then use ← → to step.";
  if (switchPane) setPane("hood");
  if (name === "bytecode") requestBytecode(getCode());
}

function setPane(name) {
  document.body.dataset.pane = name;
  for (const button of document.querySelectorAll("[data-pane-tab]")) {
    button.setAttribute("aria-pressed", String(button.dataset.paneTab === name));
  }
}

for (const tab of HOOD_TABS) {
  $(`tab-${tab}`).addEventListener("click", () => showTab(tab));
}
for (const button of document.querySelectorAll("[data-pane-tab]")) {
  button.addEventListener("click", () => setPane(button.dataset.paneTab));
}

let saveTimer = null;
let loadedFromHash = false;

function onSourceChanged() {
  // The dropdown names the example only while the code is exactly that example.
  const selected = EXAMPLES.find((e) => e.id === $("examples").value);
  if (selected && selected.source !== getCode()) $("examples").value = "";

  clearTimeout(saveTimer);
  saveTimer = setTimeout(() => storage.set(SAVE_KEY, getCode()), 300);
  // After an edit, a reload should show the edited code (autosave), not the
  // shared link's original: drop the program from the URL.
  if (loadedFromHash) {
    loadedFromHash = false;
    history.replaceState(null, "", location.pathname + location.search);
  }
  // The Bytecode tab shows the old program until the new one is compiled
  // (scheduleBytecode waits for a pause in typing): dim it meanwhile.
  $("bytecode").classList.toggle("stale", !bytecode || bytecode.source !== getCode());
  scheduleBytecode();
  if (recording && recording.source !== getCode()) {
    clearDebugLine();
    $("dbg-position").textContent = "The code has changed since this recording. Press Debug to record again.";
  }
}

$("share").addEventListener("click", async () => {
  const hash = await encodeSource(getCode());
  const url = `${location.origin}${location.pathname}#${hash}`;
  history.replaceState(null, "", url);
  loadedFromHash = true;
  try {
    await navigator.clipboard.writeText(url);
    flashButton($("share"), "Link copied");
  } catch {
    flashButton($("share"), "Link in address bar");
  }
});

function flashButton(button, text) {
  const label = button.querySelector(".label");
  const original = label.textContent;
  label.textContent = text;
  setTimeout(() => { label.textContent = original; }, 1600);
}

const THEMES = ["auto", "light", "dark"];
function applyTheme(theme) {
  if (theme === "auto") delete document.documentElement.dataset.theme;
  else document.documentElement.dataset.theme = theme;
  const name = theme[0].toUpperCase() + theme.slice(1);
  $("theme").querySelector(".theme-label").textContent = name;
  const title = theme === "auto" ? "Theme: follow the system" : `Theme: ${theme}`;
  $("theme").title = title;
  $("theme").setAttribute("aria-label", title);
}
$("theme").addEventListener("click", () => {
  const current = storage.get(THEME_KEY) || "auto";
  const next = THEMES[(THEMES.indexOf(current) + 1) % THEMES.length];
  storage.set(THEME_KEY, next);
  applyTheme(next);
});

// Ctrl/Cmd+Enter runs from anywhere on the page (the editor has its own
// binding); Esc stops a running program.
window.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && running) {
    event.preventDefault();
    stop();
  } else if (event.key === "Enter" && (event.ctrlKey || event.metaKey) && !view.hasFocus) {
    event.preventDefault();
    if (event.shiftKey) debug(); else run();
  }
}, true);

$("run").addEventListener("click", run);
$("debug").addEventListener("click", debug);
$("stop").addEventListener("click", stop);
$("clear").addEventListener("click", () => { clearOutput(); $("stats").textContent = ""; });

for (const example of EXAMPLES) {
  const option = document.createElement("option");
  option.value = example.id;
  option.textContent = example.name;
  $("examples").append(option);
}
const custom = document.createElement("option");
custom.value = "";
custom.textContent = "Your program";
custom.hidden = true;
$("examples").prepend(custom);

$("examples").addEventListener("change", () => {
  const example = EXAMPLES.find((e) => e.id === $("examples").value);
  if (example) {
    setCode(example.source);
    view.scrollDOM.scrollTop = 0;
    $("examples").value = example.id;   // setCode marked it as custom
  }
});

// The program to start with: a shared link, else the autosaved program, else
// the first example.
async function initialSource() {
  if (location.hash.length > 1) {
    try {
      const shared = await decodeSource(location.hash);
      if (shared != null) {
        loadedFromHash = true;
        return { source: shared, note: "Loaded the program from the link." };
      }
    } catch {
      return { source: storage.get(SAVE_KEY) ?? EXAMPLES[0].source,
               note: "The link's program couldn't be read (was the link cut short?)." };
    }
  }
  const saved = storage.get(SAVE_KEY);
  if (saved != null) return { source: saved };
  return { source: EXAMPLES[0].source, example: EXAMPLES[0].id };
}

// --- resizable panels -------------------------------------------------------

const LAYOUT_KEY = "clox-playground-layout";

function setupSplitters() {
  const bench = $("workbench");
  const layout = (() => { try { return JSON.parse(storage.get(LAYOUT_KEY)) || {}; } catch { return {}; } })();
  const apply = () => {
    if (layout.left) bench.style.setProperty("--left", `${layout.left}%`);
    if (layout.bottom) bench.style.setProperty("--bottom", `${layout.bottom}px`);
  };
  apply();
  const clamp = (x, lo, hi) => Math.min(Math.max(x, lo), hi);
  const save = () => storage.set(LAYOUT_KEY, JSON.stringify(layout));

  const resize = {
    // The editor's share of the width, in percent.
    v: (x, rect) => { layout.left = clamp(((x - rect.left) / rect.width) * 100, 25, 78); },
    // The Under the hood panel's height, in pixels.
    h: (y, rect) => { layout.bottom = clamp(rect.bottom - y - 12, 120, rect.height - 220); },
  };

  for (const [id, axis] of [["split-v", "v"], ["split-h", "h"]]) {
    const splitter = $(id);
    splitter.addEventListener("pointerdown", (event) => {
      event.preventDefault();
      splitter.setPointerCapture(event.pointerId);
      splitter.classList.add("dragging");
      document.body.classList.add("resizing", `resizing-${axis}`);
    });
    splitter.addEventListener("pointermove", (event) => {
      if (!splitter.hasPointerCapture(event.pointerId)) return;
      resize[axis](axis === "v" ? event.clientX : event.clientY, bench.getBoundingClientRect());
      apply();
    });
    const end = () => {
      splitter.classList.remove("dragging");
      document.body.classList.remove("resizing", `resizing-${axis}`);
      save();
    };
    splitter.addEventListener("pointerup", end);
    splitter.addEventListener("pointercancel", end);
    // Keyboard: arrows move a focused splitter; double-click resets.
    splitter.addEventListener("keydown", (event) => {
      const rect = bench.getBoundingClientRect();
      if (axis === "v" && (event.key === "ArrowLeft" || event.key === "ArrowRight")) {
        const now = layout.left ?? 56;
        layout.left = clamp(now + (event.key === "ArrowLeft" ? -2 : 2), 25, 78);
      } else if (axis === "h" && (event.key === "ArrowUp" || event.key === "ArrowDown")) {
        const now = layout.bottom ?? 300;
        layout.bottom = clamp(now + (event.key === "ArrowUp" ? 24 : -24), 120, rect.height - 220);
      } else {
        return;
      }
      event.preventDefault();
      apply();
      save();
    });
    splitter.addEventListener("dblclick", () => {
      delete layout[axis === "v" ? "left" : "bottom"];
      bench.style.removeProperty(axis === "v" ? "--left" : "--bottom");
      save();
    });
  }
}

// --- first-visit hint --------------------------------------------------------

const HINT_KEY = "clox-playground-hint-seen";

function showHintOnFirstVisit() {
  if (!storage.get(HINT_KEY)) $("hint").hidden = false;
}

function dismissHint() {
  if ($("hint").hidden) return;
  $("hint").hidden = true;
  storage.set(HINT_KEY, "1");
}

$("hint-close").addEventListener("click", dismissHint);

async function main() {
  applyTheme(storage.get(THEME_KEY) || "auto");
  const start = await initialSource();
  createEditor(start.source);
  const example = EXAMPLES.find((e) => e.source === start.source);
  $("examples").value = example ? example.id : "";
  if (start.note) appendLine("out", start.note);
  const mac = document.documentElement.dataset.platform === "mac";
  for (const el of document.querySelectorAll(".kbd-hint")) el.textContent = mac ? "(⌘↵)" : "(Ctrl+↵)";
  setupSplitters();
  startRunner();
  startHelper();
  showTab("bytecode", { switchPane: false });   // the Bytecode tab is open first
  showHintOnFirstVisit();
  updateButtons();
  // For the headless test and the browser console.
  window.playground = { getCode, setCode, view, goToStep, currentStep };
}

main();
