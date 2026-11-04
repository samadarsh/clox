# Our own Lox tests

Tests in the same `// expect:` format as the official suite. They cover cases
the official tests don't, or show a specific point from a milestone's
explanation.

| Folder | Covers |
|---|---|
| `expressions/` | precedence and associativity beyond the official `precedence.lox`, grouping, comments between tokens |
| `types/` | falsiness (`0` and `""` are truthy), `==`/`!=` across different types |
| `strings/` | concatenation, equality by content, interning of strings built at runtime |
| `variables/` | shadowing (the milestone 4 walkthrough), locals vs globals, chained assignment |
| `runtime_error/` | type errors upstream lacks (`1 + "a"`, `"a" < "b"`), error line numbers, execution stopping at the error |
| `compile_error/` | missing `)`/`;`/`}`, "Expect expression.", panic mode, recovery at the next statement, invalid assignment target |

History: at milestones 2–3 there was no `print` or `;`, so these were one
expression per file, and 21 upstream `operator/` tests were ported here with
the `;` removed. At milestone 4 the ports were deleted, since the originals now
pass from `tests/lox/operator/`. The rest were rewritten with `print`, merged,
or dropped where an official test covers the same case.

## "Error at end" and trailing newlines

Files that expect an error *at end* (on the EOF token) have **no trailing
newline**. The scanner counts a final `\n`, so with one the EOF token would sit
on the next line. Upstream does the same, as in `number/decimal_point_at_eof.lox`.
