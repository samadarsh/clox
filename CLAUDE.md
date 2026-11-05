# clox

A bytecode virtual machine for the Lox language, written in C99, following
Part III of [Crafting Interpreters](https://craftinginterpreters.com) by
Robert Nystrom. This is a portfolio project. The previous one was a Redis clone
in Go (github.com/samadarsh/redis-go), and this one keeps the same quality bar:
tests from day one, CI, a Makefile and a clear README.

## Rules

1. **Follow the book closely.** Use the book's names, file layout and code
   structure, so any chapter can be checked against the text. Deviate only
   when the toolchain requires it (for example `(void)param;` casts so that
   `-Wextra -Werror` passes), and mark every deviation with a comment
   explaining why. Deliberate changes to behaviour need the user's approval
   and are listed in README.md ("Differences from the book"). So far there is
   one: `identifierConstant()` reuses an existing name constant (chapter 21
   challenge), so many uses of a global no longer hit the 256-constant limit.
2. **Every milestone adds the matching official tests.** Copy the files from
   [munificent/craftinginterpreters/test](https://github.com/munificent/craftinginterpreters/tree/master/test)
   into `tests/lox/` with the same sub-paths, but only once clox passes them.
   Don't edit the test files.
3. **Explain every change in simple terms**, especially pointers and memory:
   who owns an allocation, when it is freed, what a pointer points to, and
   why pointer arithmetic is valid. Assume the user is learning C through this
   project.
4. **Never commit without the user's explicit approval.** Show the diff and
   test output first, then ask. Never push without asking either.

## Roadmap

| # | Milestone | Book chapters |
|---|-----------|---------------|
| 1 | Bytecode and VM | 14 Chunks of Bytecode, 15 A Virtual Machine |
| 2 | Scanner and compiler | 16 Scanning on Demand, 17 Compiling Expressions |
| 3 | Types and strings | 18 Types of Values, 19 Strings |
| 4 | Hash tables and variables | 20 Hash Tables, 21 Global Variables, 22 Local Variables |
| 5 | Control flow | 23 Jumping Back and Forth |
| 6 | Functions and closures | 24 Calls and Functions, 25 Closures |
| 7 | Garbage collector | 26 Garbage Collection |
| 8 | Classes and inheritance | 27 Classes and Instances, 28 Methods and Initializers, 29 Superclasses |
| 9 | Speed and polish | 30 Optimization, benchmarks vs Python, demo, WebAssembly playground |

When a milestone is done, tick its checkbox in README.md.

## Layout

```
src/                interpreter source, one .c/.h pair per book module
tests/unit/         C unit tests (chunk, VM stack, scanner, values/objects, table)
tests/lox/          official .lox tests, mirroring upstream test/ paths
tests/custom/       our own .lox tests (see tests/custom/README.md)
tests/cli_test.sh   REPL and exit-code tests
tests/run_tests.py  runner for "// expect: ..." style tests (same rules as test.dart)
compile_flags.txt   tells the editor's clangd where the headers are
tools/upstream_tests.py  lists the official tests a chapter should pass
bench/              upstream benchmarks (lox/), Python ports (python/), run.py
```

To find which official tests a chapter should pass, clone upstream and run
`python3 tools/upstream_tests.py <clone> chapNN_name`. It applies the rules in
upstream `tool/bin/test.dart`. Every listed test must pass. Also run the whole
upstream suite against the build to catch anything extra that passes.

## Commands

```
make build       release build (-O2, -Werror)   -> build/release/clox
make debug       ASan + UBSan build              -> build/debug/clox
make trace       debug + DEBUG_TRACE_EXECUTION   -> build/trace/clox
make test        all tests, release build
make test-asan   all tests, sanitized build
make test-stress all tests, GC before every allocation, under ASan
make gclog / make gclog-stress   builds that log GC activity
make bench       time bench/lox/*.lox (best of 5)
make bench-compare  clox vs Python 3 on bench/python/*.py
NAN_BOXING=0     add to any target for the tagged-union Value (default 1:
                 NaN boxing); its builds go in build/<mode>-union/
                 (CI runs test, test-asan and test-stress with NAN_BOXING=1 and 0;
                 run all six before asking to commit)
make clean
python3 tests/run_tests.py build/debug/clox tests/lox/<dir>   # one suite
```

## Conventions

- C99, clang, `-Wall -Wextra -Werror`. The code must also build on Linux, which
  CI checks on ubuntu-latest and macos-latest.
- All heap memory goes through `reallocate()` in `memory.c`, as in the book.
- Every heap object must be linked into `vm.objects` (`allocateObject` does
  this) so `freeVM()` and, later, the GC can find it. Unit tests that create
  objects start with `initVM()` and end with `endTest()` (test_object.c): it
  frees everything, then clears the VM's pointers *without allocating*, so any
  object freeVM missed is unreachable and LeakSanitizer reports it. Don't end
  a test with `initVM()`: since chapter 24 it allocates (the `clock` native).
- Garbage collector rule: any call that allocates (copyString, newClosure,
  tableSet, writeValueArray, ...) may run the GC, which frees every object not
  reachable from a root. A pointer held only in a C local variable is NOT a
  root. Push such objects on the VM stack (or otherwise make them reachable)
  before the next allocation, as concatenate(), allocateString(),
  addConstant(), defineNative() and bindMethod() do. `make test-stress` catches violations.
- LeakSanitizer doesn't run on macOS. Before asking to commit a milestone
  that touches memory, run `make test-asan` in a Linux container (Ubuntu with
  clang, make and python3), not only on the Mac.
- Debug flags (`DEBUG_TRACE_EXECUTION`, later `DEBUG_PRINT_CODE`,
  `DEBUG_STRESS_GC`, `DEBUG_LOG_GC`) are switched on with `-D` from the Makefile,
  not by editing `common.h`, so normal builds and tests stay clean.
- Exit codes follow the book: 65 for a compile error, 70 for a runtime error,
  74 for an I/O error.
