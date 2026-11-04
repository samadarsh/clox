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
- [ ] **3. Types and strings**: booleans, nil and heap-allocated strings (ch. 18–19)
- [ ] **4. Hash tables and variables**: global and local variables (ch. 20–22)
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

So far (milestone 2), a program is a single arithmetic expression, and clox
prints its value. Statements and `print` arrive in milestone 4.

```
> 1 + 2 * 3
7
> -(5 - (3 - 1)) / 2
-1.5
> (1 + 2
[line 2] Error at end: Expect ')' after expression.
```

Exit codes follow the book: `65` compile error, `70` runtime error,
`74` file can't be read, `64` bad usage.

`make trace` builds a version that prints the compiled bytecode and then
every instruction and the stack as the VM runs it:

```
$ ./build/trace/clox walk.lox          # 1 + 2 * 3
== code ==
0000    1 OP_CONSTANT         0 '1'
0002    | OP_CONSTANT         1 '2'
0004    | OP_CONSTANT         2 '3'
0006    | OP_MULTIPLY
0007    | OP_ADD
0008    | OP_RETURN
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
| `tests/unit/` | C unit tests for the chunk, the VM stack and the scanner |
| `tests/cli_test.sh` | the REPL and the exit codes |

## Project layout

```
src/
  scanner.c    source text -> tokens, one at a time, on demand
  compiler.c   tokens -> bytecode (single-pass Pratt parser)
  vm.c         runs bytecode on a value stack
  chunk.c      a bytecode array + line numbers + constants
  value.c      Lox values (just numbers for now)
  memory.c     every allocation goes through reallocate()
  debug.c      the disassembler
  main.c       REPL and file runner
tests/         see the table above
```

## License

MIT; see [LICENSE](LICENSE). Lox and the original clox design are by Robert
Nystrom. The test scripts in `tests/lox/` come from the
[craftinginterpreters](https://github.com/munificent/craftinginterpreters)
repository (MIT).
