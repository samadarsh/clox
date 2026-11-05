# clox

[![CI](https://github.com/samadarsh/clox/actions/workflows/ci.yml/badge.svg)](https://github.com/samadarsh/clox/actions/workflows/ci.yml)

A bytecode virtual machine for the [Lox](https://craftinginterpreters.com/the-lox-language.html)
programming language, written in C99. It follows Part III of Robert Nystrom's
[*Crafting Interpreters*](https://craftinginterpreters.com). A single-pass
compiler turns Lox source into compact bytecode, which a stack-based VM then
executes.

Every milestone is checked against the book's official test suite, and CI runs
those tests under AddressSanitizer and UndefinedBehaviorSanitizer on Linux and
macOS.

## Roadmap

- [x] **1. Bytecode and VM**: chunks, a disassembler and a stack-based VM (ch. 14–15)
- [x] **2. Scanner and compiler**: scan and compile expressions with a Pratt parser (ch. 16–17)
- [x] **3. Types and strings**: booleans, nil and heap-allocated strings (ch. 18–19)
- [x] **4. Hash tables and variables**: global and local variables (ch. 20–22)
- [x] **5. Control flow**: `if`, `while`, `for`, `and` and `or` (ch. 23)
- [x] **6. Functions and closures**: call frames, native functions and upvalues (ch. 24–25)
- [x] **7. Garbage collector**: mark-sweep with tri-color marking (ch. 26)
- [x] **8. Classes and inheritance**: instances, methods, initializers and `super` (ch. 27–29)
- [ ] **9. Speed and polish**: optimization, benchmarks vs Python, a demo and a WebAssembly playground (ch. 30)

## Building

You need clang, make and Python 3 (Python runs the tests).

```sh
make build        # optimized build -> build/release/clox
make debug        # with AddressSanitizer + UBSan -> build/debug/clox
make trace        # debug build that prints every instruction as it runs
make gclog        # debug build that logs every GC allocation, mark and free
make gclog-stress # the same, collecting before every allocation
```

Values are NaN-boxed by default (chapter 30): each one is a single 64-bit word.
Add `NAN_BOXING=0` to any target to build and test the tagged-union version
instead, e.g. `make test NAN_BOXING=0`. CI tests both.

## Usage

```sh
./build/release/clox              # start the REPL
./build/release/clox script.lox   # run a file
```

Since milestone 8, clox runs the whole Lox language: variables, blocks,
`if`/`else`, `while`, `for`, `and`/`or`, functions with `return`, recursion,
closures, classes with fields, methods and `init`, inheritance with `super`,
and the native function `clock()`. Memory is managed by a garbage collector.

```
> fun makeCounter() { var count = 0; fun inc() { count = count + 1; print count; } return inc; }
> var c = makeCounter();
> c();
1
> c();
2
> class Animal { init(name) { this.name = name; } speak() { return this.name + " makes a sound"; } }
> class Dog < Animal { speak() { return super.speak() + ": woof"; } }
> var rex = Dog("Rex");
> print rex.speak();
Rex makes a sound: woof
> var f = rex.speak;
> print f();
Rex makes a sound: woof
```

`tests/custom/programs/shapes.lox` is a longer example.

Runtime errors print a stack trace, innermost call first:

```
Operands must be two numbers or two strings.
[line 7] in inner()
[line 3] in middle()
[line 1] in outer()
[line 9] in script
```

`bench/` has the book's benchmark programs, `make bench` to time them, and
`make bench-compare` to run five of them against Python 3. Results and what
they mean are in [bench/README.md](bench/README.md).

```
> var greeting = "hello";
> print greeting + " world";
hello world
> { var greeting = "shadowed"; print greeting; }
shadowed
> print greeting;
hello
> print undefinedThing;
Undefined variable 'undefinedThing'.
[line 1] in script
```

Exit codes follow the book: `65` compile error, `70` runtime error,
`74` file can't be read, `64` bad usage.

`make trace` builds a version that prints the compiled bytecode and then
every instruction and the stack as the VM runs it:

```
$ ./build/trace/clox scope.lox    # { var a = 1; { var a = 2; print a; } print a; }
== code ==
0000    1 OP_CONSTANT         0 '1'
0002    | OP_CONSTANT         1 '2'
0004    | OP_GET_LOCAL        1
0006    | OP_PRINT
0007    | OP_POP
0008    | OP_GET_LOCAL        0
0010    | OP_PRINT
0011    | OP_POP
0012    | OP_RETURN
...
```

## Testing

```sh
make test         # unit tests + Lox test suite, release build
make test-asan    # the same, under AddressSanitizer + UBSan
make test-stress  # the same, with the GC running before EVERY allocation (+ ASan)
```

Lox tests use the same inline annotations as the
[official suite](https://github.com/munificent/craftinginterpreters/tree/master/test):

```lox
print 1 + 2;      // expect: 3
-"oops";          // expect runtime error: Operand must be a number.
```

`tests/run_tests.py` runs each `.lox` file and checks stdout, stderr and the
exit code against those comments.

`make test-stress` matters most for the garbage collector. Collecting before
every allocation frees any object the GC can't reach at the first chance, so
a forgotten root turns into an immediate use-after-free that ASan reports.
With normal GC timing, the same bug can stay silent. CI runs all three
(`test`, `test-asan`, `test-stress`) on Linux and macOS.

There are four kinds of tests:

| Where | What |
|---|---|
| `tests/lox/` | official tests from the book's repo, added once clox passes them |
| `tests/custom/` | our own `.lox` tests for what the official suite can't reach yet |
| `tests/unit/` | C unit tests: chunk, VM stack, scanner, values and heap objects, hash table, garbage collector |
| `tests/cli_test.sh` | the REPL and the exit codes |

## Project layout

```
src/
  scanner.c    source text -> tokens, one at a time, on demand
  compiler.c   tokens -> bytecode (single-pass Pratt parser)
  vm.c         runs bytecode on a value stack, one CallFrame per active call
  chunk.c      a bytecode array + line numbers + constants
  value.c      Lox values: a tagged union of bool, nil, number, object
  object.c     heap objects: strings (interned), functions, closures, upvalues,
               natives, all linked into vm.objects
  table.c      hash table: global variables and the string intern set
  memory.c     reallocate() and the mark-sweep garbage collector
  debug.c      the disassembler
  main.c       REPL and file runner
tests/         see the table above
bench/         the book's benchmarks (lox/), Python versions (python/), run.py
tools/         upstream_tests.py: which official tests a chapter should pass
```

## License

MIT; see [LICENSE](LICENSE). Lox and the original clox design are by Robert
Nystrom. The test scripts in `tests/lox/` and the benchmarks in `bench/lox/`
come from the
[craftinginterpreters](https://github.com/munificent/craftinginterpreters)
repository (MIT).
