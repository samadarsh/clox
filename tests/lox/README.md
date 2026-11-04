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

The current set is every upstream test that passes: the 153 that
`tools/upstream_tests.py <clone> chap25_closures` lists, plus
`operator/not.lox`. Upstream lists that one under `noCClasses` (skip until
classes exist), but it only uses a function (`fun foo() {}`), so it passes
from chapter 24. Running the whole upstream suite gives exactly these 154.

Still to come: the garbage collector in milestone 7 (no new tests), classes in 8.

Run one file or folder with:

    python3 tests/run_tests.py build/release/clox tests/lox/number
