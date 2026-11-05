# Official Lox tests

Test scripts copied from the book's repository,
[munificent/craftinginterpreters/test](https://github.com/munificent/craftinginterpreters/tree/master/test)
(MIT licensed, © Robert Nystrom). The directory layout matches upstream, so
`tests/lox/number/leading_dot.lox` here is `test/number/leading_dot.lox` there.
Don't edit these files.

A test is added only once clox passes it.

| Milestone | Added |
|---|---|
| 2 | `expressions/evaluate.lox`, `number/leading_dot.lox`, `string/unterminated.lox` |
| 3 | none: upstream's runner lists only `evaluate.lox` for chapters 18–19 |
| 4 | all 72 that upstream's `chap22_local` suite expects; `expressions/evaluate.lox` removed, as upstream skips `test/expressions` from chapter 21 on |
| 5 | +28 for `chap23_jumping` (100 total): `if/`, `while/`, `for/`, `logical_operator/`, `block/empty.lox`, `limit/loop_too_large.lox`, `variable/unreached_undefined.lox` |
| 6 | +54 for `chap25_closures` (154 total): `function/`, `closure/`, `call/`, `return/`, `limit/`, `regression/40.lox`, `unexpected_character.lox`, more `for/`/`while/`/`variable/`, and `operator/not.lox` (see below) |
| 7 | none: upstream's `chap26_garbage` set equals chapter 25's. A correct GC is invisible to programs, so its tests are ours: `tests/unit/test_gc.c`, `tests/custom/gc/`, and the whole suite under `make test-stress` |
| 8 | +92 for `chap29_superclasses` (246 total): `class/`, `field/`, `method/`, `constructor/`, `this/`, `inheritance/`, `super/`, and the class cases in `assignment/`, `call/`, `closure/`, `number/`, `operator/`, `regression/`, `return/`, `variable/` |

At milestone 6, `operator/not.lox` was added ahead of its chapter: upstream
lists it under `noCClasses` (skip until classes exist), but it only uses a
function (`fun foo() {}`), so it passes from chapter 24.

The set is now every upstream test clox can pass: exactly the 246 that
`tools/upstream_tests.py <clone> chap29_superclasses` lists, which is also what
upstream's final `clox` suite lists. The other 8 non-benchmark files upstream
are `scanning/*.lox` (6) and `expressions/{parse,evaluate}.lox` (2). They test
the Java interpreter's early chapters, where it prints tokens and syntax trees
instead of running the program, so upstream skips them for clox too. Running
them against clox gives 0 passed, 8 failed, as it should. `benchmark/` is
skipped as well: those are timing programs, not tests. They are in
`bench/lox/` instead (see `bench/README.md`).

Run one file or folder with:

    python3 tests/run_tests.py build/release/clox tests/lox/number
