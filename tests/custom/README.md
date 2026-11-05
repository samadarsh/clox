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
| `control_flow/` | nested `if`/`else`, short-circuit `and`/`or` (the right side must not run), every `for` clause missing, zero-iteration loops, stack balance over 1000 iterations, jumps over more than 255 bytes |
| `functions/` | recursion, mutual recursion, the exact call-depth limit (63 calls OK, 64 is "Stack overflow."), wrong argument counts, calling non-functions, errors in nested calls, functions as values, `clock()` |
| `closures/` | the counter walkthrough, two closures sharing one variable, loop variable capture, closing at block end and at return, three-level capture |
| `gc/` | lots of garbage (string building in loops), closures and open/closed upvalues surviving collections, interned strings freed and recreated, GC during deep calls and during compiling, instances/classes/bound methods keeping what they point to alive |
| `classes/` | fields added after creation, bound methods keeping `this` (stored in fields, passed as callbacks), `init` returning `this` (also on early `return;`), calling `init` again directly, returning a value from `init` (error), inheriting from an instance, a string or itself (errors), `super` at every level of a 3-class chain, the constant limit for methods (127 fit, 128 is "Too many constants in one chunk.") |
| `programs/` | longer programs using many features together |

The GC tests do their real work under `make test-stress`, which collects before
every allocation. `tests/unit/test_gc.c` checks the collector directly: what
gets freed, the weak string table, the threshold, and memory staying bounded
while a program creates about 5 MB of garbage.

The full multi-frame stack trace (every frame and its line, not just the first)
is checked in `tests/cli_test.sh`, since the `.lox` runner, like upstream's,
only compares the first trace line.

The 16-bit jump limit itself ("Too much code to jump over.", at exactly
65,536 bytes) is tested in `tests/cli_test.sh`, which generates the ~130 KB
programs while it runs instead of storing them here.

History: at milestones 2–3 there was no `print` or `;`, so these were one
expression per file, and 21 upstream `operator/` tests were ported here with
the `;` removed. At milestone 4 the ports were deleted, since the originals now
pass from `tests/lox/operator/`. The rest were rewritten with `print`, merged,
or dropped where an official test covers the same case.

## "Error at end" and trailing newlines

Files that expect an error *at end* (on the EOF token) have **no trailing
newline**. The scanner counts a final `\n`, so with one the EOF token would sit
on the next line. Upstream does the same, as in `number/decimal_point_at_eof.lox`.
