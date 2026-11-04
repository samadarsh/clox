# Our own Lox tests

Tests we wrote ourselves, in the same `// expect:` format as the official
suite. They cover behaviour the official tests can't check yet. At milestone 2
almost every official test needs `print`, which arrives in chapter 21.

- `expressions/`: precedence, associativity, grouping, unary minus, number
  literals. Each file is a single expression, because a program is just one
  expression until chapter 21. clox prints the value of that expression.
- `compile_error/`: syntax errors, such as a missing `)`, "Expect expression.",
  unexpected characters, and panic mode suppressing follow-up errors.

## Temporary tests

When chapter 21 adds statements, programs stop printing the value of a bare
expression and "Expect end of expression." goes away. At that point:

- `expressions/*` will be rewritten with `print` (most will then duplicate
  the official `precedence.lox`, `number/literals.lox` etc. and can be deleted)
- `compile_error/extra_token.lox` and `compile_error/empty_file.lox` will be
  updated or removed

## "Error at end" and trailing newlines

Files that expect an error *at end* (on the EOF token) have **no trailing
newline**. The scanner counts a final `\n`, so with one the EOF token would sit
on the next line. Upstream does the same, as in `number/decimal_point_at_eof.lox`.
