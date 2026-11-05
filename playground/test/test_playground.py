#!/usr/bin/env python3
"""Headless browser test for the clox playground.

Serves playground/ on a free local port, opens it in headless Chromium with
Playwright, and runs programs through the real page: the Run button, the Web
Worker, the WebAssembly clox, and the output panel.

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

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)
        print(f"  FAIL {message}")
    return condition


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass  # keep the test output to the results


def serve():
    handler = functools.partial(QuietHandler, directory=PLAYGROUND)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def output_lines(page):
    return page.locator("#output .line").all_inner_texts()


def run_and_wait(page, timeout_ms=20000):
    page.click("#run")
    # Finished, compile error, runtime error or crash: anything but running.
    page.wait_for_function(
        "() => !['running', ''].includes(document.getElementById('status').dataset.kind)"
        " && document.getElementById('status').textContent !== 'Ready'",
        timeout=timeout_ms)
    # Output is rendered on the next animation frame after the last message.
    page.wait_for_timeout(100)
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


def main():
    if not os.path.exists(os.path.join(PLAYGROUND, "clox.wasm")):
        sys.exit("playground/clox.wasm is missing: run `make wasm` first")

    server = serve()
    url = f"http://127.0.0.1:{server.server_address[1]}/index.html"

    with sync_playwright() as p:
        browser = p.chromium.launch()
        page = browser.new_page()
        console_errors = []
        page.on("pageerror", lambda e: console_errors.append(str(e)))
        page.goto(url)
        page.wait_for_function(
            "() => document.getElementById('status').textContent === 'Ready'",
            timeout=20000)
        print("page loaded, worker ready")

        # Every example in the dropdown, through the real page.
        for example_id, (exit_code, expected) in EXPECTED.items():
            page.select_option("#examples", example_id)
            status, lines = run_and_wait(page)
            ok = check(matches(expected, lines),
                       f"{example_id}: output {lines!r}")
            ok &= check(status.startswith(STATUS_FOR_EXIT[exit_code]),
                        f"{example_id}: status {status!r}")
            if example_id == "fib" and ok:
                # clock() works: the measured time is a real, positive number.
                ok &= check(float(lines[2]) > 0, f"fib: clock() gave {lines[2]}")
            print(f"  {'ok  ' if ok else 'FAIL'} example {example_id}: {status}")

        # Errors carry line numbers, and "[line N]" links select that line.
        page.select_option("#examples", "errors")
        run_and_wait(page)
        page.locator("#output a", has_text="[line 2]").first.click()
        selected = page.evaluate(
            "() => editor.value.slice(editor.selectionStart, editor.selectionEnd)")
        check("return x + \"one\";" in selected,
              f"error link: selected {selected!r}")
        print("  ok   [line N] link selects the line")

        # A compile error: message with line number, exit code 65.
        page.fill("#editor", "print 1;\nprint (1 + ;\n")
        status, lines = run_and_wait(page)
        check(lines == ["[line 2] Error at ';': Expect expression."],
              f"compile error: output {lines!r}")
        check(status.startswith("Compile error"), f"compile error: status {status!r}")
        print(f"  ok   compile error: {status}")

        # Each Run starts from a fresh VM: nothing survives from the last run.
        page.fill("#editor", "var leftover = 1;\nprint leftover;\n")
        status, lines = run_and_wait(page)
        check(lines == ["1"], f"fresh VM, first run: {lines!r}")
        page.fill("#editor", "print leftover;\n")
        status, lines = run_and_wait(page)
        check(lines == ["Undefined variable 'leftover'.", "[line 1] in script"],
              f"fresh VM, second run: {lines!r}")
        print("  ok   each run starts from a fresh VM")

        # An endless loop doesn't freeze the page, and Stop ends it.
        page.select_option("#examples", "forever")
        page.click("#run")
        page.wait_for_timeout(1000)
        # The page must still respond while the worker is busy.
        responsive = page.evaluate("() => 1 + 1") == 2
        check(responsive, "page unresponsive during an infinite loop")
        check(page.locator("#stop").is_enabled(), "Stop not enabled while running")
        page.click("#stop")
        page.wait_for_function(
            "() => document.getElementById('status').textContent === 'Ready'",
            timeout=20000)
        check("[stopped]" in output_lines(page), "no [stopped] line after Stop")
        # ...and the fresh worker runs the next program normally.
        page.select_option("#examples", "hello")
        status, lines = run_and_wait(page)
        check(lines == EXPECTED["hello"][1], f"after Stop: {lines!r}")
        print("  ok   Stop ends an infinite loop; the next run works")

        check(not console_errors, f"JavaScript errors: {console_errors}")
        browser.close()

    server.shutdown()
    if failures:
        print(f"{len(failures)} playground check(s) failed")
        sys.exit(1)
    print("PASS (playground)")


if __name__ == "__main__":
    main()
