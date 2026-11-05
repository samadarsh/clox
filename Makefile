# clox — build and test.
#
#   make build        optimized build                   -> build/release/clox
#   make debug        ASan + UBSan, debug info          -> build/debug/clox
#   make trace        debug + bytecode + instruction trace -> build/trace/clox
#   make stress       debug + GC on EVERY allocation    -> build/stress/clox
#   make gclog        debug + GC log (normal GC timing) -> build/gclog/clox
#   make gclog-stress debug + GC log + GC on every allocation
#                                                       -> build/gclog-stress/clox
#   make test         run all tests against the release build
#   make test-asan    run all tests against the sanitized debug build
#   make test-stress  run all tests with GC on every allocation, under ASan
#   make wasm         WebAssembly build for the browser playground (needs emcc)
#                       -> playground/clox.js + playground/clox.wasm
#   make test-wasm    run the playground in headless Chromium (needs Playwright)
#   make wasm-docker / make test-wasm-docker
#                     the same two, inside pinned Docker images (what CI runs),
#                     so neither emcc nor Playwright has to be installed
#   make bench        time the book's benchmarks (bench/lox) with the release build
#   make bench-compare  the same programs in clox and Python 3 (bench/python)
#   make clean        delete build/
#
# Values are NaN-boxed by default (chapter 30). Add NAN_BOXING=0 to any target
# to use the tagged-union representation instead, e.g. `make test-asan
# NAN_BOXING=0`. Those builds go in build/<mode>-union/, so the two
# representations never share object files.
#
# CI runs test, test-asan and test-stress, with NAN_BOXING=1 and NAN_BOXING=0.
#
# Works with the GNU Make 3.81 that ships with macOS.

CC   = clang
STD  = -std=c99
WARN = -Wall -Wextra -Werror

RELEASE_FLAGS = -O2
SANITIZE      = -fsanitize=address,undefined -fno-sanitize-recover=all
DEBUG_FLAGS   = -g -O0 -fno-omit-frame-pointer $(SANITIZE)

# MODE picks which flavour every rule below builds.
MODE ?= release
ifeq ($(MODE),release)
  FLAGS = $(RELEASE_FLAGS)
else ifeq ($(MODE),debug)
  FLAGS = $(DEBUG_FLAGS)
else ifeq ($(MODE),trace)
  FLAGS = $(DEBUG_FLAGS) -DDEBUG_PRINT_CODE -DDEBUG_TRACE_EXECUTION
else ifeq ($(MODE),stress)
  FLAGS = $(DEBUG_FLAGS) -DDEBUG_STRESS_GC
else ifeq ($(MODE),gclog)
  FLAGS = $(DEBUG_FLAGS) -DDEBUG_LOG_GC
else ifeq ($(MODE),gclog-stress)
  FLAGS = $(DEBUG_FLAGS) -DDEBUG_LOG_GC -DDEBUG_STRESS_GC
else
  $(error Unknown MODE '$(MODE)': use release, debug, trace, stress, gclog or gclog-stress)
endif

# NaN boxing on (1, the default, as in the book) or off (0).
NAN_BOXING ?= 1
ifeq ($(NAN_BOXING),1)
  VALUE_FLAGS = -DNAN_BOXING
  FLAGS += $(VALUE_FLAGS)
  OUT    = build/$(MODE)
else ifeq ($(NAN_BOXING),0)
  OUT    = build/$(MODE)-union
else
  $(error NAN_BOXING must be 1 or 0, not '$(NAN_BOXING)')
endif

CFLAGS = $(STD) $(WARN) $(FLAGS)

SRC      = $(wildcard src/*.c)
OBJ      = $(SRC:src/%.c=$(OUT)/%.o)
LIB_OBJ  = $(filter-out $(OUT)/main.o,$(OBJ))   # everything except main()
CLOX     = $(OUT)/clox

UNIT_SRC = $(wildcard tests/unit/*.c)
UNIT_BIN = $(UNIT_SRC:tests/unit/%.c=$(OUT)/tests/%)

.PHONY: all build debug trace stress gclog gclog-stress test test-asan test-stress \
        test-unit test-cli test-lox bench bench-compare wasm test-wasm \
        wasm-docker test-wasm-docker clean

all: build

build: $(CLOX)

debug:
	@$(MAKE) --no-print-directory MODE=debug build

trace:
	@$(MAKE) --no-print-directory MODE=trace build

stress:
	@$(MAKE) --no-print-directory MODE=stress build

gclog:
	@$(MAKE) --no-print-directory MODE=gclog build

gclog-stress:
	@$(MAKE) --no-print-directory MODE=gclog-stress build

# --- compiling --------------------------------------------------------------

$(CLOX): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

# -MMD -MP writes a .d file per object listing the headers it includes, so
# editing a header rebuilds everything that depends on it.
$(OUT)/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUT)/tests/%: tests/unit/%.c $(LIB_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -Isrc $^ -o $@

-include $(OBJ:.o=.d)

# --- testing ----------------------------------------------------------------

test: test-unit test-cli test-lox

test-asan:
	@$(MAKE) --no-print-directory MODE=debug test

# The garbage collector runs before every allocation, under ASan. Any object
# the GC can't reach is freed at the first chance, so a missing root or an
# unprotected temporary turns into an immediate use-after-free that ASan
# reports, instead of a rare crash.
test-stress:
	@$(MAKE) --no-print-directory MODE=stress test

# C unit tests: chunk, value array, VM stack, scanner, values and objects, table, GC.
test-unit: $(UNIT_BIN)
	@for t in $(UNIT_BIN); do echo "--- $$t"; $$t || exit 1; done

# Command-line behaviour: REPL, exit codes for missing files and bad usage.
test-cli: $(CLOX)
	@echo "--- cli"
	@sh tests/cli_test.sh $(CLOX)

# tests/lox:    official Crafting Interpreters tests, added as milestones pass them
# tests/custom: our own .lox tests for cases the official suite can't cover yet
test-lox: $(CLOX)
	@echo "--- lox tests (official + custom)"
	@python3 tests/run_tests.py $(CLOX) tests/lox tests/custom

# --- benchmarks -------------------------------------------------------------

# Best of 5 runs each. Run on a quiet machine: other work slows the timings.
bench: $(CLOX)
	@python3 bench/run.py $(CLOX)

PYTHON ?= python3

bench-compare: $(CLOX)
	@python3 bench/run.py --python $(PYTHON) $(CLOX)

# --- WebAssembly playground ---------------------------------------------------

# The interpreter sources without main.c, plus playground/clox_wasm.c as the
# entry point. Built with Emscripten's emcc, which compiles C to WebAssembly.
#   MODULARIZE + EXPORT_NAME  clox.js defines createClox(), which makes a new
#                             instance (fresh memory, fresh VM) on each call
#   ENVIRONMENT=worker        it only ever runs inside a Web Worker
#   ALLOW_MEMORY_GROWTH       the heap grows as Lox programs allocate
#   STACK_SIZE                1 MB of C stack (the compiler recurses on nested
#                             expressions)
#   CLOX_PLAYGROUND           switches on the hooks in playground/hooks.h
#                             (instruction counts, GC stats, the debugger's
#                             recording). Only this build defines it, and only
#                             this build has playground/ on the include path.
EMCC       ?= emcc
WASM_SRC    = $(filter-out src/main.c,$(SRC)) playground/clox_wasm.c
WASM_OUT    = playground/clox.js
WASM_FLAGS  = $(STD) $(WARN) -O2 $(VALUE_FLAGS) -DCLOX_PLAYGROUND \
              -Isrc -Iplayground \
              -sMODULARIZE=1 -sEXPORT_NAME=createClox -sENVIRONMENT=worker \
              -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 \
              -sEXPORTED_FUNCTIONS=_clox_run,_clox_disassemble,_clox_debug,_clox_recording,_clox_recording_full,_clox_stat_instructions,_clox_stat_collections,_clox_stat_peak_bytes,_malloc,_free \
              -sEXPORTED_RUNTIME_METHODS=stringToNewUTF8,UTF8ToString

wasm: $(WASM_OUT)

$(WASM_OUT): $(WASM_SRC) $(wildcard src/*.h) playground/hooks.h
	$(EMCC) $(WASM_FLAGS) $(WASM_SRC) -o $@

# Serves playground/ on a local port and drives it with headless Chromium.
test-wasm: $(WASM_OUT)
	@echo "--- playground (headless Chromium)"
	@python3 playground/test/test_playground.py

# Pinned tool versions for the Docker targets (and so for CI).
EMSDK_IMAGE        = emscripten/emsdk:6.0.13
PLAYWRIGHT_VERSION = 1.64.1
PLAYWRIGHT_IMAGE   = mcr.microsoft.com/playwright/python:v$(PLAYWRIGHT_VERSION)-noble

wasm-docker:
	docker run --rm -v "$(CURDIR)":/src -w /src $(EMSDK_IMAGE) make wasm

# The image has the browsers; the Python package is installed at the same
# version, so the two match.
test-wasm-docker:
	@test -f playground/clox.wasm || { echo "run make wasm (or make wasm-docker) first"; exit 1; }
	docker run --rm -v "$(CURDIR)":/src -w /src $(PLAYWRIGHT_IMAGE) bash -c \
	  "pip install -q --break-system-packages playwright==$(PLAYWRIGHT_VERSION) && \
	   python3 playground/test/test_playground.py"

clean:
	rm -rf build playground/clox.js playground/clox.wasm
