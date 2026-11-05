#!/usr/bin/env python3
"""Headless browser test for the clox playground.

Serves playground/ on a free local port, opens it in headless Chromium with
Playwright, and uses it like a person would: the editor, Run, Debug, Stop,
the Output, Bytecode and Debugger tabs, Share, and the theme toggle. Every
program really runs through the Web Worker and the WebAssembly clox.

Needs `make wasm` first, and Playwright for Python:
    pip install playwright==1.64.1 && python3 -m playwright install chromium
Run:
    python3 playground/test/test_playground.py
"""

import functools
import http.server
import os
import re
import sys
import threading

from playwright.sync_api import sync_playwright

PLAYGROUND = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Expected output of each example, line by line. A compiled regex matches a
# line whose exact text varies (a timing).
NUMBER = re.compile(r"^\d+(\.\d+)?(e-\d+)?$")
EXPECTED = {
    "hello": (0, ["Hello, world!", "Hello, Lox!", "7"]),
    "fib": (0, ["832040", "seconds:", NUMBER]),
    "closures": (0, ["1", "2", "3", "1"]),
    "classes": (0, [
        "Cat makes a sound.",
        "Rex makes a sound. Woof!",
        "Rex Jr. makes a sound. Woof! (squeaky)",
        "Fido makes a sound. Woof!",
    ]),
    "fizzbuzz": (0, ["1", "2", "Fizz", "4", "Buzz", "Fizz", "7", "8", "Fizz",
                     "Buzz", "11", "Fizz", "13", "14", "FizzBuzz"]),
    "errors": (70, [
        "before the error",
        "Operands must be two numbers or two strings.",
        "[line 2] in inner()",
        "[line 6] in outer()",
        "[line 10] in script",
    ]),
}

STATUS_FOR_EXIT = {0: "Finished in", 65: "Compile error", 70: "Runtime error"}

# A small program with a function call, for the debugger. Its 13 steps:
#   script: 0 CLOSURE  1 DEFINE_GLOBAL  2 GET_GLOBAL  3 CONSTANT  4 CONSTANT  5 CALL
#   add:    6 GET_LOCAL  7 GET_LOCAL  8 ADD  9 RETURN
#   script: 10 PRINT  11 NIL  12 RETURN
CALL_PROGRAM = "fun add(a, b) {\n  return a + b;\n}\nprint add(1, 2);\n"

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)
        print(f"  FAIL {message}")
    return condition


def passed(label):
    print(f"  ok   {label}")


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass  # keep the test output to the results


def serve():
    handler = functools.partial(QuietHandler, directory=PLAYGROUND)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


# --- helpers ------------------------------------------------------------------

def wait_ready(page):
    page.wait_for_function(
        "() => document.getElementById('status').textContent === 'Ready'",
        timeout=20000)


def set_code(page, text):
    page.evaluate("text => playground.setCode(text)", text)


def get_code(page):
    return page.evaluate("() => playground.getCode()")


def output_lines(page):
    return page.locator("#output .line").all_inner_texts()


def wait_finished(page, timeout_ms=20000):
    # Finished, errored, stopped or crashed: anything but running.
    page.wait_for_function(
        "() => { const s = document.getElementById('status');"
        " return !['running', ''].includes(s.dataset.kind) && s.textContent !== 'Ready'; }",
        timeout=timeout_ms)
    page.wait_for_timeout(150)   # output is drawn on the next animation frame


def run_and_wait(page, button="#run"):
    page.click(button)
    wait_finished(page)
    return page.locator("#status").inner_text(), output_lines(page)


def matches(expected, actual):
    if len(expected) != len(actual):
        return False
    for want, got in zip(expected, actual):
        if isinstance(want, re.Pattern):
            if not want.match(got):
                return False
        elif want != got:
            return False
    return True


def editor_line_with(page, css_class):
    """1-based line numbers of editor lines that have `css_class`."""
    return page.evaluate("""cls => {
        const lines = [...document.querySelectorAll('.cm-content .cm-line')];
        return lines.map((el, i) => el.classList.contains(cls) ? i + 1 : 0).filter(Boolean);
    }""", css_class)


def debugger_state(page):
    return {
        "position": page.locator("#dbg-position").inner_text(),
        "stack": page.locator("#dbg-stack li .value").all_inner_texts(),
        "frames": page.locator("#dbg-frames li").all_inner_texts(),
        "current": page.locator("#dbg-code .bc-op.current").inner_text(),
        "output": page.locator("#dbg-output").inner_text(),
        "debug_line": editor_line_with(page, "cm-debug-line"),
    }


# --- the tests ----------------------------------------------------------------

def test_examples(page):
    for example_id, (exit_code, expected) in EXPECTED.items():
        page.select_option("#examples", example_id)
        status, lines = run_and_wait(page)
        ok = check(matches(expected, lines), f"{example_id}: output {lines!r}")
        ok &= check(status.startswith(STATUS_FOR_EXIT[exit_code]),
                    f"{example_id}: status {status!r}")
        if example_id == "fib" and ok:
            # clock() works: the measured time is a real, positive number.
            ok &= check(float(lines[2]) > 0, f"fib: clock() gave {lines[2]}")
        if ok:
            passed(f"example {example_id}: {status}")


def test_stats(page):
    page.select_option("#examples", "hello")
    run_and_wait(page)
    stats = page.locator("#stats").inner_text().replace("\n", " ")
    # "Hello, world" is exactly 18 instructions (see the Bytecode tab).
    ok = check("18 instructions" in stats, f"stats: {stats!r}")
    ok &= check("0 GC collections" in stats and "peak heap" in stats and " ms" in stats,
                f"stats: {stats!r}")
    if ok:
        passed(f"stats bar: {stats}")


def test_line_links(page):
    page.select_option("#examples", "errors")
    run_and_wait(page)
    page.locator("#output a", has_text="[line 2]").first.click()
    selected = page.evaluate(
        "() => { const s = playground.view.state.selection.main;"
        " return playground.view.state.sliceDoc(s.from, s.to); }")
    if check('return x + "one";' in selected, f"error link selected {selected!r}"):
        passed("[line N] link in the output selects the line")


def test_errors_in_editor(page):
    # A compile error: the exact token is underlined, the message on hover.
    set_code(page, "print 1;\nprint (1 + ;\n")
    status, lines = run_and_wait(page)
    check(lines == ["[line 2] Error at ';': Expect expression."], f"compile error: {lines!r}")
    check(status.startswith("Compile error"), f"compile error status {status!r}")
    underline = page.locator(".cm-lintRange-error")
    ok = check(underline.count() == 1, f"compile error: {underline.count()} underlines")
    if ok:
        ok &= check(underline.inner_text() == ";", f"underlined {underline.inner_text()!r}")
        underline.hover()
        tooltip = page.locator(".cm-tooltip-lint")
        tooltip.wait_for(timeout=5000)
        ok &= check("Expect expression." in tooltip.inner_text(),
                    f"tooltip {tooltip.inner_text()!r}")
    if ok:
        passed("compile error underlined in the editor, message on hover")
    page.mouse.move(0, 0)

    # A runtime error: its line is underlined, with the message and function.
    page.select_option("#examples", "errors")
    run_and_wait(page)
    underline = page.locator(".cm-lintRange-error")
    # The code is underlined, its trailing comment isn't.
    ok = check(underline.count() == 1 and underline.inner_text() == 'return x + "one";',
               f"runtime error underline: {underline.all_inner_texts()!r}")
    if ok:
        underline.hover()
        tooltip = page.locator(".cm-tooltip-lint")
        tooltip.wait_for(timeout=5000)
        ok &= check("Operands must be two numbers or two strings. (in inner())"
                    in tooltip.inner_text(), f"tooltip {tooltip.inner_text()!r}")
    if ok:
        passed("runtime error underlined in the editor, message on hover")
    page.mouse.move(0, 0)


def test_fresh_vm(page):
    set_code(page, "var leftover = 1;\nprint leftover;\n")
    _, lines = run_and_wait(page)
    check(lines == ["1"], f"fresh VM, first run: {lines!r}")
    set_code(page, "print leftover;\n")
    _, lines = run_and_wait(page)
    if check(lines == ["Undefined variable 'leftover'.", "[line 1] in script"],
             f"fresh VM, second run: {lines!r}"):
        passed("each run starts from a fresh VM")


def test_stop(page):
    # Stop button: an endless loop doesn't freeze the page.
    page.select_option("#examples", "forever")
    page.click("#run")
    page.wait_for_timeout(800)
    check(page.evaluate("() => 1 + 1") == 2, "page unresponsive during an infinite loop")
    check(page.locator("#stop").is_enabled(), "Stop not enabled while running")
    page.click("#stop")
    wait_ready(page)
    check("[stopped]" in output_lines(page), "no [stopped] line after Stop")
    # Esc does the same.
    page.click("#run")
    page.wait_for_timeout(500)
    page.keyboard.press("Escape")
    wait_ready(page)
    check(page.locator("#status").inner_text() == "Ready", "Esc didn't stop the program")
    # ...and the fresh worker runs the next program normally.
    page.select_option("#examples", "hello")
    _, lines = run_and_wait(page)
    if check(lines == EXPECTED["hello"][1], f"after Stop: {lines!r}"):
        passed("Stop and Esc end an infinite loop; the next run works")


def test_keyboard_run(page):
    set_code(page, 'print "from the keyboard";\n')
    page.click(".cm-content")
    page.keyboard.press("Control+Enter")
    wait_finished(page)
    if check(output_lines(page) == ["from the keyboard"], f"Ctrl+Enter: {output_lines(page)!r}"):
        passed("Ctrl+Enter runs the program")


def test_share_and_autosave(browser, url):
    # Share puts the program in the URL...
    context = browser.new_context()
    page = context.new_page()
    page.goto(url)
    wait_ready(page)
    program = 'var who = "a shared link";\nprint "Hello from " + who + "!";\n'
    set_code(page, program)
    page.click("#share")
    page.wait_for_function("() => location.hash.startsWith('#code=')", timeout=5000)
    shared_url = page.url
    context.close()

    # ...and a browser that has never seen it opens the link with that code.
    context = browser.new_context()
    page = context.new_page()
    page.goto(shared_url)
    wait_ready(page)
    ok = check(get_code(page) == program, f"shared link loaded {get_code(page)!r}")
    _, lines = run_and_wait(page)
    ok &= check(lines == ["Loaded the program from the link.", "Hello from a shared link!"]
                or lines == ["Hello from a shared link!"], f"shared program output {lines!r}")
    if ok:
        passed(f"Share: the code survives the URL ({len(shared_url.split('#')[1])} characters)")

    # Autosave: an edit survives a reload (and replaces the link's program).
    set_code(page, 'print "autosaved";\n')
    page.wait_for_timeout(600)
    check("#code=" not in page.url, "the link's program stayed in the URL after an edit")
    page.reload()
    wait_ready(page)
    if check(get_code(page) == 'print "autosaved";\n', f"after reload: {get_code(page)!r}"):
        passed("autosave restores the program after a reload")
    context.close()


def test_bytecode_tab(page):
    set_code(page, CALL_PROGRAM)
    page.click("#tab-bytecode")
    # The tab is always on screen; wait until it shows this program, not the last one.
    page.wait_for_selector("#bytecode:not(.stale) .bc-op", timeout=10000)
    headings = page.locator("#bytecode h3").all_inner_texts()
    headings = [" ".join(h.split()) for h in headings]
    ok = check(headings == ["<script> (top-level code)", "fun add"], f"headings {headings!r}")
    rows = page.locator("#bytecode .bc-op").all_inner_texts()
    # 9 in the script, 6 in add (including the implicit `return nil`).
    ok &= check(len(rows) == 15, f"{len(rows)} bytecode rows")
    ok &= check(any("OP_CALL" in r for r in rows) and any("OP_ADD" in r for r in rows),
                f"rows {rows!r}")

    # Hovering an instruction highlights its source line...
    page.locator("#bytecode .bc-op", has_text="OP_ADD").hover()
    ok &= check(editor_line_with(page, "cm-hover-line") == [2],
                f"hovered OP_ADD, highlighted lines {editor_line_with(page, 'cm-hover-line')}")
    # ...and hovering a source line highlights its instructions.
    page.locator(".cm-content .cm-line").nth(3).hover()   # line 4
    linked = page.locator("#bytecode .bc-op.linked").all_inner_texts()
    ok &= check(len(linked) >= 5 and all(r.startswith(("0004", "0006", "0008", "0010", "0012", "0013"))
                                         or "|" in r for r in linked) and any("OP_CALL" in r for r in linked),
                f"line 4 linked rows {linked!r}")
    if ok:
        passed("bytecode tab: 2 functions, 15 instructions, hover links both ways")
    page.mouse.move(0, 0)


def test_debugger(page):
    set_code(page, CALL_PROGRAM)
    page.click("#debug")
    wait_finished(page)
    check(page.locator("#tab-debugger").get_attribute("aria-selected") == "true",
          "Debug didn't open the Debugger tab")
    page.wait_for_selector("#dbg-code .bc-op.current", timeout=10000)

    state = debugger_state(page)
    ok = check(state["position"].startswith("Step 1 of 13"), f"start: {state['position']!r}")
    ok &= check(state["frames"] == ["script  line 3"], f"start frames {state['frames']!r}")
    ok &= check("OP_CLOSURE" in state["current"], f"start instruction {state['current']!r}")

    # Step forward into add(): a second frame appears, with the arguments.
    for _ in range(6):
        page.click("#dbg-forward")
    state = debugger_state(page)
    ok &= check(state["position"].startswith("Step 7 of 13"), f"step 7: {state['position']!r}")
    ok &= check(state["frames"] == ["add()  line 2", "script  line 4"], f"step 7 frames {state['frames']!r}")
    ok &= check(state["stack"] == ["<script>", "<fn add>", "1", "2"], f"step 7 stack {state['stack']!r}")
    ok &= check(state["debug_line"] == [2], f"step 7 editor line {state['debug_line']}")
    ok &= check("OP_GET_LOCAL" in state["current"], f"step 7 instruction {state['current']!r}")

    # Two more: about to add, with both operands pushed.
    page.click("#dbg-forward")
    page.click("#dbg-forward")
    state = debugger_state(page)
    ok &= check("OP_ADD" in state["current"], f"step 9 instruction {state['current']!r}")
    ok &= check(state["stack"] == ["<script>", "<fn add>", "1", "2", "1", "2"],
                f"step 9 stack {state['stack']!r}")
    tab_row = page.locator("#bytecode .bc-op.current").inner_text()
    ok &= check("OP_ADD" in tab_row, f"Bytecode tab current row {tab_row!r}")

    # Step back out of the function: one frame again, at the call on line 4.
    for _ in range(3):
        page.click("#dbg-back")
    state = debugger_state(page)
    ok &= check(state["position"].startswith("Step 6 of 13"), f"back: {state['position']!r}")
    ok &= check(state["frames"] == ["script  line 4"], f"back frames {state['frames']!r}")
    ok &= check("OP_CALL" in state["current"], f"back instruction {state['current']!r}")
    ok &= check(state["debug_line"] == [4], f"back editor line {state['debug_line']}")

    # The end: the result was printed.
    page.click("#dbg-last")
    state = debugger_state(page)
    ok &= check(state["position"].startswith("Step 13 of 13"), f"last: {state['position']!r}")
    ok &= check(state["output"] == "3", f"output so far {state['output']!r}")
    page.click("#dbg-first")
    ok &= check(debugger_state(page)["output"] == "", "output at step 1 isn't empty")
    if ok:
        passed("debugger: steps into add() and back out; stack, frames, lines and output follow")


def test_debugger_limit(page):
    set_code(page, "while (true) {}\n")
    page.click("#debug")
    wait_finished(page)
    banner = page.locator("#dbg-banner")
    ok = check(banner.is_visible() and "20,000 steps" in banner.inner_text(),
               f"limit banner {banner.inner_text()!r}")
    ok &= check(page.locator("#dbg-slider").get_attribute("max") == "19999", "slider max")
    ok &= check("limit" in page.locator("#status").inner_text(), "status at the limit")
    if ok:
        passed("debugging an endless loop stops at 20,000 steps, with a message")


def test_theme(page):
    page.click("#theme")
    theme1 = page.evaluate("() => document.documentElement.dataset.theme || 'auto'")
    page.click("#theme")
    theme2 = page.evaluate("() => document.documentElement.dataset.theme || 'auto'")
    page.click("#theme")
    theme3 = page.evaluate("() => document.documentElement.dataset.theme || 'auto'")
    if check([theme1, theme2, theme3] == ["light", "dark", "auto"],
             f"theme cycle {[theme1, theme2, theme3]}"):
        passed("theme toggle: auto -> light -> dark -> auto")


def test_phone_layout(browser, url):
    context = browser.new_context(viewport={"width": 390, "height": 844}, is_mobile=True)
    page = context.new_page()
    page.goto(url)
    wait_ready(page)
    overflow = page.evaluate("() => document.documentElement.scrollWidth - window.innerWidth")
    _, lines = run_and_wait(page)
    if check(overflow <= 0 and lines == EXPECTED["hello"][1],
             f"phone: horizontal overflow {overflow}px, output {lines!r}"):
        passed("phone-sized screen: no sideways scrolling, Run works")
    context.close()


def main():
    if not os.path.exists(os.path.join(PLAYGROUND, "clox.wasm")):
        sys.exit("playground/clox.wasm is missing: run `make wasm` first")

    server = serve()
    url = f"http://127.0.0.1:{server.server_address[1]}/index.html"

    with sync_playwright() as p:
        browser = p.chromium.launch()
        context = browser.new_context(viewport={"width": 1400, "height": 900})
        page = context.new_page()
        page_errors = []
        page.on("pageerror", lambda e: page_errors.append(str(e)))
        page.goto(url)
        wait_ready(page)
        print("page loaded, worker ready")

        test_examples(page)
        test_stats(page)
        test_line_links(page)
        test_errors_in_editor(page)
        test_fresh_vm(page)
        test_stop(page)
        test_keyboard_run(page)
        test_bytecode_tab(page)
        test_debugger(page)
        test_debugger_limit(page)
        test_theme(page)
        test_share_and_autosave(browser, url)
        test_phone_layout(browser, url)

        check(not page_errors, f"JavaScript errors: {page_errors}")
        browser.close()

    server.shutdown()
    if failures:
        print(f"{len(failures)} playground check(s) failed")
        sys.exit(1)
    print("PASS (playground)")


if __name__ == "__main__":
    main()
