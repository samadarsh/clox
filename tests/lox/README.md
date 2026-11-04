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

The current set is exactly what `tools/upstream_tests.py <clone> chap22_local`
prints. Running the *whole* upstream suite also gives exactly 72 passes, so
nothing passes beyond what the book expects at this chapter.

Still to come: control flow (`if`, `while`, `for`, `and`/`or`) in milestone 5,
functions and closures in 6, classes in 8.

Run one file or folder with:

    python3 tests/run_tests.py build/release/clox tests/lox/number
