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

Most official tests use `print` and statements, which arrive in chapter 21
(milestone 4). Until then, only these three pass.

`expressions/` is only for the chapters before statements exist. From chapter
21 onwards, upstream's runner skips it (`earlyChapters` in `tool/bin/test.dart`),
and so will we.

Run one file or folder with:

    python3 tests/run_tests.py build/release/clox tests/lox/number
