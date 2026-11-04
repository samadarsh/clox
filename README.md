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
- [ ] **2. Scanner and compiler**: scan and compile expressions with a Pratt parser (ch. 16–17)
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

At milestone 1 there is no compiler yet. `clox` runs a hand-built chunk of
bytecode for `-((1.2 + 3.4) / 5.6)`:

```
$ ./build/release/clox
== test chunk ==
0000  123 OP_CONSTANT         0 '1.2'
0002    | OP_CONSTANT         1 '3.4'
0004    | OP_ADD
0005    | OP_CONSTANT         2 '5.6'
0007    | OP_DIVIDE
0008    | OP_NEGATE
0009    | OP_RETURN
-0.821429
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
exit code against those comments. Official tests are added to `tests/lox/`
as each milestone starts passing them.

## Project layout

```
src/          the interpreter (chunk, value, memory, debug, vm, main)
tests/unit/   C unit tests for internal data structures
tests/lox/    official Lox test scripts
tests/run_tests.py
```

## License

MIT; see [LICENSE](LICENSE). Lox and the original clox design are by Robert
Nystrom. The test scripts in `tests/lox/` come from the
[craftinginterpreters](https://github.com/munificent/craftinginterpreters)
repository (MIT).
