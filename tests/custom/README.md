# Our own Lox tests

Tests in the same `// expect:` format as the official suite. They cover
behaviour the official tests can't check yet, because almost every official
test needs `print` or `;`, which arrive in chapter 21 (milestone 4).

Until then a program is a single expression, so each file holds one
expression and clox prints its value.

| Folder | Covers | Since |
|---|---|---|
| `expressions/` | precedence, associativity, grouping, unary minus, number literals | milestone 2 |
| `compile_error/` | missing `)`, "Expect expression.", unexpected character, panic mode | milestone 2 |
| `types/` | `true`/`false`/`nil`, `!`, falsiness, `==`/`!=` across types, comparisons | milestone 3 |
| `strings/` | string literals, concatenation, equality by content, multi-line, unicode | milestone 3 |
| `runtime_error/` | type errors (exit 70, message and line) | milestone 3 |

## Ported from upstream

21 files in `runtime_error/` (`add_bool_nil.lox`, `negate_nonnum.lox`,
`less_num_nonnum.lox`, …) are copies of upstream `test/operator/*.lox` with
the trailing `;` removed. Each file says so in its first lines. When the
official files pass in milestone 4, add them to `tests/lox/operator/` and
delete these copies.

## Temporary tests

When chapter 21 adds statements, programs stop printing the value of a bare
expression, and "Expect end of expression." goes away. At that point:

- `expressions/`, `types/`, `strings/` and `runtime_error/` get rewritten with
  `print` and `;`, or deleted where an official test now covers the same thing
- `compile_error/extra_token.lox` and `compile_error/empty_file.lox` are
  updated or removed

## "Error at end" and trailing newlines

Files that expect an error *at end* (on the EOF token) have **no trailing
newline**. The scanner counts a final `\n`, so with one the EOF token would sit
on the next line. Upstream does the same, as in `number/decimal_point_at_eof.lox`.
