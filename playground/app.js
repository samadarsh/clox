// The playground page: editor, examples, Run/Stop, output. The interpreter
// itself runs in worker.js.

const editor = document.getElementById("editor");
const gutter = document.getElementById("gutter");
const examples = document.getElementById("examples");
const runButton = document.getElementById("run");
const stopButton = document.getElementById("stop");
const clearButton = document.getElementById("clear");
const statusText = document.getElementById("status");
const output = document.getElementById("output");

let worker = null;
let workerReady = false;
let running = false;

// --- worker ----------------------------------------------------------------

function startWorker() {
  workerReady = false;
  worker = new Worker("worker.js");
  worker.onmessage = (event) => handleMessage(event.data);
  worker.onerror = (event) => {
    setStatus(`Worker error: ${event.message}`, "error");
    running = false;
    updateButtons();
  };
}

function handleMessage(message) {
  switch (message.type) {
    case "ready":
      workerReady = true;
      if (!running) setStatus("Ready", "");
      updateButtons();
      break;
    case "output":
      for (const [stream, text] of message.chunks) appendLine(stream, text);
      break;
    case "done": {
      running = false;
      const ms = message.ms < 10 ? message.ms.toFixed(1) : Math.round(message.ms);
      if (message.exitCode === 0) {
        setStatus(`Finished in ${ms} ms`, "ok");
      } else if (message.exitCode === 65) {
        setStatus("Compile error (exit code 65)", "error");
      } else {
        setStatus("Runtime error (exit code 70)", "error");
      }
      updateButtons();
      break;
    }
    case "crash":
      running = false;
      appendLine("err", message.message);
      setStatus("Crashed", "error");
      updateButtons();
      break;
  }
}

function run() {
  if (!workerReady || running) return;
  clearOutput();
  running = true;
  setStatus("Running…", "running");
  updateButtons();
  worker.postMessage({ type: "run", source: editor.value });
}

// A busy worker can't be interrupted, only thrown away: terminate it and
// start a new one for the next run.
function stop() {
  if (!running) return;
  worker.terminate();
  running = false;
  appendLine("err", "[stopped]");
  setStatus("Stopped", "error");
  startWorker();
  updateButtons();
}

// --- output ----------------------------------------------------------------

// Lines arrive in batches; render them once per frame, not once per line.
let queue = [];
let scheduled = false;

function appendLine(stream, text) {
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
  for (const [stream, text] of queue) {
    const line = document.createElement("div");
    line.className = stream === "err" ? "line err" : "line";
    if (stream === "err") {
      addLineLinks(line, text);
    } else {
      line.textContent = text;
    }
    fragment.appendChild(line);
  }
  queue = [];
  output.appendChild(fragment);
  if (atBottom) output.scrollTop = output.scrollHeight;
}

// Turn every "[line N]" in an error into a link that selects line N.
function addLineLinks(element, text) {
  const pattern = /\[line (\d+)\]/g;
  let last = 0;
  for (const match of text.matchAll(pattern)) {
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
  output.textContent = "";
}

function setStatus(text, kind) {
  statusText.textContent = text;
  statusText.dataset.kind = kind;
}

function updateButtons() {
  runButton.disabled = !workerReady || running;
  stopButton.disabled = !running;
}

// --- editor ----------------------------------------------------------------

function updateGutter() {
  const lines = editor.value.split("\n").length;
  let text = "";
  for (let i = 1; i <= lines; i++) text += i + "\n";
  gutter.textContent = text;
  gutter.scrollTop = editor.scrollTop;
}

function selectLine(number) {
  const lines = editor.value.split("\n");
  if (number < 1 || number > lines.length) return;
  let start = 0;
  for (let i = 0; i < number - 1; i++) start += lines[i].length + 1;
  editor.focus();
  editor.setSelectionRange(start, start + lines[number - 1].length);
  // Scroll the line into view: roughly, by line height.
  const lineHeight = parseFloat(getComputedStyle(editor).lineHeight) || 20;
  editor.scrollTop = Math.max(0, (number - 3) * lineHeight);
}

editor.addEventListener("input", updateGutter);
editor.addEventListener("scroll", () => { gutter.scrollTop = editor.scrollTop; });

editor.addEventListener("keydown", (event) => {
  if (event.key === "Enter" && (event.ctrlKey || event.metaKey)) {
    event.preventDefault();
    run();
  } else if (event.key === "Tab" && !event.shiftKey) {
    // Indent with two spaces instead of leaving the editor.
    event.preventDefault();
    editor.setRangeText("  ", editor.selectionStart, editor.selectionEnd, "end");
    updateGutter();
  }
});

for (const example of EXAMPLES) {
  const option = document.createElement("option");
  option.value = example.id;
  option.textContent = example.name;
  examples.appendChild(option);
}

function loadExample(id) {
  const example = EXAMPLES.find((e) => e.id === id) || EXAMPLES[0];
  examples.value = example.id;
  editor.value = example.source;
  editor.scrollTop = 0;
  updateGutter();
}

examples.addEventListener("change", () => loadExample(examples.value));
runButton.addEventListener("click", run);
stopButton.addEventListener("click", stop);
clearButton.addEventListener("click", clearOutput);

loadExample("hello");
startWorker();
updateButtons();
