#!/usr/bin/env python3
"""Run .lox test files and check their output against inline expectations.

Usage: run_tests.py <path/to/clox> <test dir or .lox file>... [--filter TEXT]

Understands the same annotations as the book's official test runner
(tool/bin/test.dart in munificent/craftinginterpreters):

  // expect: <text>                 a line the script must print to stdout
  // expect runtime error: <msg>    first stderr line; exit code 70; stderr
                                    must also contain "[line N]" for the line
                                    carrying the comment
  // Error at 'x': <msg>            compile error on this line; exit code 65
  // [line N] Error ...             compile error reported on line N
  // [c line N] Error ...           as above, but only for clox
  // [java line N] Error ...        jlox only, ignored here
  // nontest                        the file is not a test; skip it

Only the standard library is used, so it runs on any machine with Python 3.
"""

import argparse
import os
import re
import subprocess
import sys

LANGUAGE = "c"

EXPECTED_OUTPUT = re.compile(r"// expect: ?(.*)")
EXPECTED_ERROR = re.compile(r"// (Error.*)")
ERROR_LINE = re.compile(r"// \[((java|c) )?line (\d+)\] (Error.*)")
EXPECTED_RUNTIME_ERROR = re.compile(r"// expect runtime error: (.+)")
SYNTAX_ERROR = re.compile(r"\[.*line (\d+)\] (Error.+)")
STACK_TRACE = re.compile(r"\[line (\d+)\]")
NON_TEST = re.compile(r"// nontest")

EXIT_COMPILE_ERROR = 65
EXIT_RUNTIME_ERROR = 70


class Test:
    def __init__(self, path):
        self.path = path
        self.expected_output = []  # list of (line number, text)
        self.expected_errors = set()
        self.runtime_error = None
        self.runtime_error_line = 0
        self.exit_code = 0
        self.failures = []

    def parse(self):
        """Read expectations from the file. Returns False if it isn't a test."""
        with open(self.path, encoding="utf-8") as f:
            lines = f.read().split("\n")

        for line_num, line in enumerate(lines, start=1):
            if NON_TEST.search(line):
                return False

            m = EXPECTED_OUTPUT.search(line)
            if m:
                self.expected_output.append((line_num, m.group(1)))
                continue

            m = EXPECTED_ERROR.search(line)
            if m:
                self.expected_errors.add(f"[line {line_num}] {m.group(1)}")
                self.exit_code = EXIT_COMPILE_ERROR
                continue

            m = ERROR_LINE.search(line)
            if m:
                language = m.group(2)
                if language is None or language == LANGUAGE:
                    self.expected_errors.add(f"[line {m.group(3)}] {m.group(4)}")
                    self.exit_code = EXIT_COMPILE_ERROR
                continue

            m = EXPECTED_RUNTIME_ERROR.search(line)
            if m:
                self.runtime_error_line = line_num
                self.runtime_error = m.group(1)
                self.exit_code = EXIT_RUNTIME_ERROR

        if self.expected_errors and self.runtime_error:
            self.fail("Test error: cannot expect both compile and runtime errors.")
        return True

    def run(self, interpreter):
        try:
            proc = subprocess.run(
                [interpreter, self.path],
                capture_output=True,
                text=True,
                errors="replace",
                timeout=10,
            )
        except subprocess.TimeoutExpired:
            self.fail("Timed out after 10 seconds.")
            return

        out_lines = proc.stdout.split("\n")
        err_lines = proc.stderr.split("\n")
        # A trailing newline produces one empty last element; drop it.
        if out_lines and out_lines[-1] == "":
            out_lines.pop()
        if err_lines and err_lines[-1] == "":
            err_lines.pop()

        if self.runtime_error:
            self.check_runtime_error(err_lines)
        else:
            self.check_compile_errors(err_lines)
        self.check_exit_code(proc.returncode, err_lines)
        self.check_output(out_lines)

    def check_runtime_error(self, err_lines):
        if not err_lines:
            self.fail(f"Expected runtime error '{self.runtime_error}' and got none.")
            return
        if err_lines[0] != self.runtime_error:
            self.fail(f"Expected runtime error '{self.runtime_error}' and got:")
            self.fail(err_lines[0])

        trace_line = None
        for line in err_lines[1:]:
            m = STACK_TRACE.search(line)
            if m:
                trace_line = int(m.group(1))
                break
        if trace_line is None:
            self.fail("Expected stack trace and got:", err_lines[1:])
        elif trace_line != self.runtime_error_line:
            self.fail(f"Expected runtime error on line {self.runtime_error_line} "
                      f"but was on line {trace_line}.")

    def check_compile_errors(self, err_lines):
        found = set()
        unexpected = []
        for line in err_lines:
            m = SYNTAX_ERROR.search(line)
            if m:
                error = f"[line {m.group(1)}] {m.group(2)}"
                if error in self.expected_errors:
                    found.add(error)
                else:
                    unexpected.append(f"Unexpected error: {line}")
            elif line != "":
                unexpected.append(f"Unexpected output on stderr: {line}")

        for msg in unexpected[:10]:
            self.fail(msg)
        if len(unexpected) > 10:
            self.fail(f"(and {len(unexpected) - 10} more...)")
        for error in sorted(self.expected_errors - found):
            self.fail(f"Missing expected error: {error}")

    def check_exit_code(self, code, err_lines):
        if code == self.exit_code:
            return
        if code < 0:
            self.fail(f"Interpreter crashed with signal {-code}.", err_lines[:10])
        else:
            self.fail(f"Expected return code {self.exit_code} and got {code}. "
                      "Stderr:", err_lines[:10])

    def check_output(self, out_lines):
        index = 0
        for line in out_lines:
            if index >= len(self.expected_output):
                self.fail(f"Got output '{line}' when none was expected.")
                index += 1
                continue
            line_num, expected = self.expected_output[index]
            if expected != line:
                self.fail(f"Expected output '{expected}' on line {line_num} "
                          f"and got '{line}'.")
            index += 1

        for line_num, expected in self.expected_output[index:]:
            self.fail(f"Missing expected output '{expected}' on line {line_num}.")

    def fail(self, message, lines=None):
        self.failures.append(message)
        for line in lines or []:
            self.failures.append(f"    {line}")


def find_tests(paths):
    tests = []
    for path in paths:
        if os.path.isfile(path):
            tests.append(path)
            continue
        for root, dirs, files in os.walk(path):
            dirs.sort()
            for name in sorted(files):
                if name.endswith(".lox"):
                    tests.append(os.path.join(root, name))
    return tests


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("interpreter", help="path to the clox binary")
    parser.add_argument("paths", nargs="+", help=".lox files or directories")
    parser.add_argument("--filter", default="", help="only run paths containing this text")
    args = parser.parse_args()

    if not os.access(args.interpreter, os.X_OK):
        print(f"Interpreter not found or not executable: {args.interpreter}")
        return 2

    passed = failed = skipped = 0
    for path in find_tests(args.paths):
        if args.filter not in path:
            continue
        test = Test(path)
        if not test.parse():
            skipped += 1
            continue
        if not test.failures:
            test.run(args.interpreter)
        if test.failures:
            failed += 1
            print(f"FAIL {path}")
            for msg in test.failures:
                print(f"     {msg}")
        else:
            passed += 1

    total = passed + failed
    if total == 0:
        print("No .lox tests yet (official tests are added as milestones pass them).")
        return 0

    print(f"{passed} passed, {failed} failed, {skipped} skipped")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
