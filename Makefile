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
#   make clean        delete build/
#
# CI runs test, test-asan and test-stress.
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

CFLAGS = $(STD) $(WARN) $(FLAGS)
OUT    = build/$(MODE)

SRC      = $(wildcard src/*.c)
OBJ      = $(SRC:src/%.c=$(OUT)/%.o)
LIB_OBJ  = $(filter-out $(OUT)/main.o,$(OBJ))   # everything except main()
CLOX     = $(OUT)/clox

UNIT_SRC = $(wildcard tests/unit/*.c)
UNIT_BIN = $(UNIT_SRC:tests/unit/%.c=$(OUT)/tests/%)

.PHONY: all build debug trace stress gclog gclog-stress test test-asan test-stress \
        test-unit test-cli test-lox clean

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

clean:
	rm -rf build
