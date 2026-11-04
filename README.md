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
- [ ] **5. Control flow**: `if`, `while`, `for`, `and` and `or` (ch. 23)
- [ ] **6. Functions and closures**: call frames, native functions and upvalues (ch. 24–25)
- [ ] **7. Garbage collector**: mark-sweep with tri-color marking (ch. 26)
- [ ] **8. Classes and inheritance**: instances, methods, initializers and `super` (ch. 27–29)
- [ ] **9. Speed and polish**: optimization, benchmarks vs Python, a demo and a WebAssembly playground (ch. 30)

## Building

You need clang, make and Python 3 (Python runs the tests).

```sh
make build        # optimized build -> build/release/clox
make debug        # with AddressSanitizer + UBSan -> build/debug/clox
make trace        # debug build that prints every instruction as it runs
```

## Usage

```sh
./build/release/clox              # start the REPL
./build/release/clox script.lox   # run a file
```

So far (milestone 4), clox runs statements: `print`, expression statements,
`var` declarations, assignment and `{ }` blocks with local variables. Values
are numbers, booleans, `nil` and strings. Control flow arrives in milestone 5.

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
```

Lox tests use the same inline annotations as the
[official suite](https://github.com/munificent/craftinginterpreters/tree/master/test):

```lox
print 1 + 2;      // expect: 3
-"oops";          // expect runtime error: Operand must be a number.
```

`tests/run_tests.py` runs each `.lox` file and checks stdout, stderr and the
exit code against those comments. There are four kinds of tests:

| Where | What |
|---|---|
| `tests/lox/` | official tests from the book's repo, added once clox passes them |
| `tests/custom/` | our own `.lox` tests for what the official suite can't reach yet |
| `tests/unit/` | C unit tests: chunk, VM stack, scanner, values and heap objects, hash table |
| `tests/cli_test.sh` | the REPL and the exit codes |

## Project layout

```
src/
  scanner.c    source text -> tokens, one at a time, on demand
  compiler.c   tokens -> bytecode (single-pass Pratt parser)
  vm.c         runs bytecode on a value stack
  chunk.c      a bytecode array + line numbers + constants
  value.c      Lox values: a tagged union of bool, nil, number, object
  object.c     heap objects (strings, interned), all linked into vm.objects
  table.c      hash table: global variables and the string intern set
  memory.c     every allocation goes through reallocate()
  debug.c      the disassembler
  main.c       REPL and file runner
tests/         see the table above
tools/         upstream_tests.py: which official tests a chapter should pass
```

## License

MIT; see [LICENSE](LICENSE). Lox and the original clox design are by Robert
Nystrom. The test scripts in `tests/lox/` come from the
[craftinginterpreters](https://github.com/munificent/craftinginterpreters)
repository (MIT).
