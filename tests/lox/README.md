# Official Lox tests

Test scripts copied from the book's repository,
[munificent/craftinginterpreters/test](https://github.com/munificent/craftinginterpreters/tree/master/test)
(MIT licensed, © Robert Nystrom). The directory layout matches upstream, so
`tests/lox/operator/add.lox` here is `test/operator/add.lox` there.

We add tests only once clox passes them. Milestone 1 has no scanner or
compiler, so this directory is empty for now. The first tests arrive with
milestone 2 (chapters 16–17).

Run them with `make test-lox`, or run a single file or folder:

    python3 tests/run_tests.py build/release/clox tests/lox/operator
