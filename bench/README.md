# Benchmarks

```sh
make bench           # the book's benchmarks with the release build
make bench-compare   # five of them in clox and in Python 3
python3 bench/run.py build/release/clox other/clox   # compare two builds
```

Every program runs 5 times and the best time counts. Times are wall-clock
seconds for the whole process, startup included. `run.py` also checks that
every build (and each Python port) prints the same results.

| Folder | What |
|---|---|
| `lox/` | upstream's `test/benchmark/*.lox`, unchanged, plus `string_equality_locals.lox` (below) |
| `python/` | Python 3 versions of `fib`, `method_call`, `string_equality_locals`, `instantiation` and `equality`, line for line |

`lox/string_equality.lox` doesn't compile in clox, the book's own version
included: every use of a global adds the name to the chunk's constants again,
and its hundreds of uses go past the 256-constant limit ("Too many constants in
one chunk."). `string_equality_locals.lox` is the same program inside a block,
so the variables are locals and need no constants.

## Chapter 30: before and after

Apple M3 Pro, Apple clang 21, `-O2`. "baseline" is milestone 8 (`8e25e74`),
"bitmask" adds the hash table change (the tagged-union build), "NaN boxing"
adds NaN boxing too (the default build).

| Benchmark | baseline | bitmask | NaN boxing | total |
|---|---|---|---|---|
| binary_trees | 1.416 s | 1.415 s | 1.249 s | 1.13x |
| equality | 2.985 s | 2.994 s | 2.871 s | 1.04x |
| fib | 0.857 s | 0.852 s | 0.775 s | 1.10x |
| instantiation | 0.619 s | 0.616 s | 0.633 s | 0.98x |
| invocation | 0.198 s | 0.197 s | 0.225 s | 0.88x |
| method_call | 0.138 s | 0.128 s | 0.132 s | 1.05x |
| properties | 0.343 s | 0.286 s | 0.315 s | 1.09x |
| string_equality_locals | 0.855 s | 0.814 s | 0.729 s | 1.17x |
| trees | 1.785 s | 1.869 s | 1.626 s | 1.10x |
| zoo | 0.250 s | 0.222 s | 0.240 s | 1.04x |
| zoo_batch (batches in 10 s, higher is better) | 6,424 | 7,001 | 6,581 | 1.02x |

- The bitmask helps where hash lookups dominate: `properties` 1.20x, `zoo`
  1.13x, `zoo_batch` 1.09x (fields and methods are found by hashing).
- NaN boxing helps number-heavy and allocation-heavy code (`fib`,
  `binary_trees`, `trees`, `string_equality_locals`: 1.10x to 1.17x), but makes
  the method-call benchmarks about 10% slower than the bitmask-only build on
  this machine. About half of that is the compiler: with NaN boxing, clang
  stops inlining `invokeFromClass()` into `run()`. The rest is the cost of
  getting at objects: checking a NaN-boxed object's type and extracting its
  pointer takes two 64-bit masks, where the union compares a small tag.

## clox vs Python 3.14

| Benchmark | clox | Python | Faster |
|---|---|---|---|
| equality | 2.842 s | 2.365 s | Python 1.20x |
| fib | 0.765 s | 0.678 s | Python 1.13x |
| instantiation | 0.638 s | 0.727 s | clox 1.14x |
| method_call | 0.131 s | 0.151 s | clox 1.15x |
| string_equality_locals | 0.728 s | 0.528 s | Python 1.38x |

Python 3.14.5 from Homebrew (built with `--enable-optimizations`, i.e.
profile-guided optimization; its experimental JIT is off), same machine. clox
is the default NaN-boxing build. Some context:

- **Startup.** Python takes 18 ms to start, clox 2 ms. That is part of clox's
  lead on the short benchmarks: by the programs' own clocks, clox is 1.09x
  faster on `instantiation` and 1.08x on `method_call`.
- **`equality` is a win for Python only because CPython deletes code.** Its
  compiler drops statements that are just a constant (`1; nil; "str";`), so the
  first of the two loops does almost nothing (0.30 s, against clox's 1.24 s).
  In the second loop, which really compares values, clox is faster: 1.52 s
  against 2.05 s.
- **`fib`: Python's calls and globals are specialized.** Since 3.11, CPython
  rewrites its bytecode as it runs into specialized forms: a call to a plain
  Python function, a global read that skips the hash lookup by checking a
  cached dictionary version, integer `+` and `<`. clox looks up `fib` in the
  globals hash table on every call.
- **`string_equality_locals`: Python's instruction dispatch is cheaper.** Most
  of this program is reading locals and throwing them away. CPython jumps
  straight from one instruction's code to the next ("computed goto"), keeps its
  instruction pointer in a local variable, and fuses some instruction pairs.
  clox goes back through one `switch`, and reads and writes `frame->ip` in
  memory for every byte (see the profile).
- **Where clox wins**, creating instances and calling methods: clox values are
  copied without any bookkeeping, while CPython changes a reference count
  every time a value is pushed, popped or stored, and its object creation goes
  through the more general `__new__`/`__init__` machinery.

CPython is a production interpreter with decades of tuning; clox is the book's
teaching implementation. Being in the same range on these programs is mostly
a sign that a compact bytecode VM is a good design, not that clox is "faster
than Python" in general.
