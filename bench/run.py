#!/usr/bin/env python3
"""Benchmark runner for clox.

Compare clox builds on the book's benchmarks (bench/lox/*.lox):
  python3 bench/run.py build/release/clox other/clox ...

Compare clox with Python 3 (bench/python/*.py, each next to bench/lox/<same name>.lox):
  python3 bench/run.py --python python3 build/release/clox

Each program runs --runs times (default 5) and the best time is kept: the
fastest run is the one least disturbed by other work on the machine. Times are
wall-clock seconds for the whole process, so they include startup and
compiling (a few milliseconds).

zoo_batch is different: it runs for a fixed 10 seconds and prints how many
batches it finished, so for it the best (highest) batch count is reported.
"""

import argparse
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
LOX_DIR = os.path.join(HERE, "lox")
PY_DIR = os.path.join(HERE, "python")

# Programs whose result is a count printed second to last, not a time.
THROUGHPUT = {"zoo_batch"}


class Failed(Exception):
    pass


def run_once(cmd):
    start = time.perf_counter()
    result = subprocess.run(cmd, capture_output=True, text=True)
    elapsed = time.perf_counter() - start
    if result.returncode == 65:
        # A compile error is a result too (string_equality.lox, see
        # string_equality_locals.lox): show it in the table.
        raise Failed("compile error")
    if result.returncode != 0:
        sys.exit(f"{' '.join(cmd)} failed ({result.returncode}):\n{result.stderr}")
    return elapsed, result.stdout.splitlines()


def measure(cmd, name, runs):
    """Best of `runs`. Returns (value, output lines without timing lines)."""
    best = None
    for _ in range(runs):
        elapsed, lines = run_once(cmd)
        if name in THROUGHPUT:
            value = int(float(lines[-2]))
            best = value if best is None else max(best, value)
        else:
            best = elapsed if best is None else min(best, elapsed)
    return best, lines


def answer(lines):
    """The lines a correct program must print, without the timings.

    The benchmarks print results (true, counts, labels like "loop") and
    timings from clock(). Timings are the lines that are non-whole numbers.
    Comparing the rest checks that every build, and each Python port, computed
    the same thing.
    """
    def is_timing(line):
        try:
            return float(line) != int(float(line))
        except ValueError:
            return False
    return [line for line in lines if not is_timing(line)]


def fmt(name, value):
    return f"{value:,} batches" if name in THROUGHPUT else f"{value:7.3f} s"


def compare_builds(binaries, runs):
    names = sorted(f[:-4] for f in os.listdir(LOX_DIR) if f.endswith(".lox"))
    labels = [os.path.relpath(b) for b in binaries]
    header = f"{'benchmark':24}" + "".join(f"{l[-22:]:>24}" for l in labels)
    if len(binaries) >= 2:
        header += f"{'last/first':>12}"
    print(header)
    print("-" * len(header))
    for name in names:
        path = os.path.join(LOX_DIR, name + ".lox")
        results = []   # (value, lines), or None where a build can't compile it
        for binary in binaries:
            try:
                results.append(measure([binary, path], name, runs))
            except Failed:
                results.append(None)
        ran = [r for r in results if r is not None]
        # zoo_batch's printed sum depends on how many batches fit in 10 s.
        if name not in THROUGHPUT:
            answers = [answer(lines) for _, lines in ran]
            if any(a != answers[0] for a in answers):
                sys.exit(f"{name}: builds disagree: {answers}")
        row = f"{name:24}" + "".join(
            f"{'compile error' if r is None else fmt(name, r[0]):>24}" for r in results)
        if len(binaries) >= 2 and results[0] and results[-1]:
            a, b = results[0][0], results[-1][0]
            ratio = b / a if name in THROUGHPUT else a / b
            row += f"{ratio:11.2f}x"
        print(row, flush=True)


def compare_python(python, clox, runs):
    names = sorted(f[:-3] for f in os.listdir(PY_DIR) if f.endswith(".py"))
    version = subprocess.run([python, "--version"], capture_output=True,
                             text=True).stdout.strip()
    print(f"clox: {os.path.relpath(clox)}   python: {version}   best of {runs}")
    header = f"{'benchmark':24}{'clox':>10}{'python':>10}   faster"
    print(header)
    print("-" * (len(header) + 12))
    for name in names:
        lox = measure([clox, os.path.join(LOX_DIR, name + ".lox")], name, runs)
        py = measure([python, os.path.join(PY_DIR, name + ".py")], name, runs)
        if answer(lox[1]) != answer(py[1]):
            sys.exit(f"{name}: clox printed {answer(lox[1])!r}, "
                     f"python printed {answer(py[1])!r}")
        c, p = lox[0], py[0]
        winner = f"clox {p / c:.2f}x" if c < p else f"python {c / p:.2f}x"
        print(f"{name:24}{c:9.3f}s{p:9.3f}s   {winner}", flush=True)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--python", metavar="PYTHON",
                        help="compare one clox binary with this Python")
    parser.add_argument("binaries", nargs="+")
    args = parser.parse_args()

    if args.python:
        if len(args.binaries) != 1:
            parser.error("--python takes exactly one clox binary")
        compare_python(args.python, args.binaries[0], args.runs)
    else:
        compare_builds(args.binaries, args.runs)


if __name__ == "__main__":
    main()
