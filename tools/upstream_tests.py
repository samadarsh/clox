#!/usr/bin/env python3
"""List the official tests a clox chapter is expected to pass.

Reads the book's own test runner config (tool/bin/test.dart in a checkout of
munificent/craftinginterpreters) and applies the same rules: walk each test's
path, and the deepest folder or file named in the chapter's map decides "pass"
or "skip". Benchmarks are always skipped.

Usage:
  git clone --depth 1 https://github.com/munificent/craftinginterpreters /tmp/ci
  python3 tools/upstream_tests.py /tmp/ci chap22_local
Prints one path per line, relative to upstream's test/ directory.
"""

import os
import re
import sys


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    repo, suite = sys.argv[1], sys.argv[2]
    src = open(os.path.join(repo, "tool", "bin", "test.dart"), encoding="utf-8").read()

    entry = re.compile(r'"([^"]+)":\s*"(pass|skip)"')
    shared = {m.group(1): dict(entry.findall(m.group(2)))
              for m in re.finditer(r"var (\w+) = \{(.*?)\n  \};", src, re.S)}

    m = re.search(r'c\("' + re.escape(suite) + r'", \{(.*?)\n  \}\);', src, re.S)
    if not m:
        print(f"No clox suite named {suite!r} in test.dart")
        return 1

    # Apply entries in source order, so later ones override earlier ones, as
    # in the Dart map literal.
    rules = {}
    for tok in re.finditer(r'\.\.\.(\w+)|"([^"]+)":\s*"(pass|skip)"', m.group(1)):
        if tok.group(1):
            rules.update(shared[tok.group(1)])
        else:
            rules[tok.group(2)] = tok.group(3)

    found = []
    for root, _, files in os.walk(os.path.join(repo, "test")):
        for name in files:
            if not name.endswith(".lox"):
                continue
            path = os.path.relpath(os.path.join(root, name), repo).replace(os.sep, "/")
            if "benchmark" in path:
                continue
            state, sub = None, ""
            for part in path.split("/"):
                sub = part if not sub else sub + "/" + part
                state = rules.get(sub, state)
            if state == "pass":
                found.append(path[len("test/"):])

    for path in sorted(found):
        print(path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
