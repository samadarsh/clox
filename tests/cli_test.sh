#!/bin/sh
# Tests clox's command-line behaviour: the REPL and the exit codes.
# Usage: tests/cli_test.sh path/to/clox

CLOX="$1"
if [ ! -x "$CLOX" ]; then
  echo "usage: $0 path/to/clox" >&2
  exit 2
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

failed=0
passed=0

# check NAME EXPECTED_EXIT EXPECTED_STDOUT EXPECTED_STDERR -- runs the command
# that was stored in $TMP/cmd and compares its exit code and output.
check() {
  name=$1; want_code=$2; want_out=$3; want_err=$4
  sh "$TMP/cmd" > "$TMP/out" 2> "$TMP/err"
  code=$?
  out=$(cat "$TMP/out")
  err=$(cat "$TMP/err")
  if [ "$code" = "$want_code" ] && [ "$out" = "$want_out" ] && [ "$err" = "$want_err" ]; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
    echo "FAIL $name"
    echo "     exit:   want $want_code, got $code"
    echo "     stdout: want [$want_out]"
    echo "             got  [$out]"
    echo "     stderr: want [$want_err]"
    echo "             got  [$err]"
  fi
}

nl='
'

# The REPL prints "> ", evaluates each line, and prints a newline at EOF.
# A compile error is reported but the REPL keeps going.
# Each REPL line is compiled on its own, starting from line 1. fgets keeps the
# line's '\n', which the scanner counts, so an error "at end" says line 2.
printf 'print 1 + 2 * 3;\nprint (1 + 2\nprint -4 / 2;\n' > "$TMP/input"
echo "\"$CLOX\" < \"$TMP/input\"" > "$TMP/cmd"
check "repl evaluates lines and survives errors" 0 \
  "> 7${nl}> > -2${nl}> " \
  "[line 2] Error at end: Expect ')' after expression."

# Globals live in the VM, not in one line's chunk, so they persist between
# REPL lines. A runtime error doesn't end the REPL either.
printf 'var a = "one";\nprint b;\nvar b = a + "two";\nprint b;\n' > "$TMP/input"
echo "\"$CLOX\" < \"$TMP/input\"" > "$TMP/cmd"
check "repl keeps globals between lines" 0 \
  "> > > > onetwo${nl}> " \
  "Undefined variable 'b'.${nl}[line 1] in script"

# Empty input: just the prompt, then the newline printed on EOF.
echo "\"$CLOX\" < /dev/null" > "$TMP/cmd"
check "repl exits cleanly on immediate EOF" 0 "> " ""

# Running a file.
printf 'print (1 + 2) * 3;\n' > "$TMP/ok.lox"
echo "\"$CLOX\" \"$TMP/ok.lox\"" > "$TMP/cmd"
check "runs a file" 0 "9" ""

# Compile error in a file: exit 65.
printf '1 +' > "$TMP/bad.lox"
echo "\"$CLOX\" \"$TMP/bad.lox\"" > "$TMP/cmd"
check "compile error exits 65" 65 "" "[line 1] Error at end: Expect expression."

# Runtime error in a file: message, then the line, exit 70.
printf '1 +\n"a";\n' > "$TMP/rt.lox"
echo "\"$CLOX\" \"$TMP/rt.lox\"" > "$TMP/cmd"
check "runtime error exits 70" 70 "" \
  "Operands must be two numbers or two strings.${nl}[line 2] in script"

# Missing file: exit 74.
echo "\"$CLOX\" \"$TMP/does-not-exist.lox\"" > "$TMP/cmd"
check "missing file exits 74" 74 "" "Could not open file \"$TMP/does-not-exist.lox\"."

# Too many arguments: usage message, exit 64.
echo "\"$CLOX\" a b" > "$TMP/cmd"
check "too many arguments exits 64" 64 "" "Usage: clox [path]"

# A runtime error deep in a call chain prints one stack trace line per frame,
# innermost first, each with the line that frame was executing.
cat > "$TMP/trace.lox" <<'EOF'
fun outer() { middle(); }
fun middle() {
  inner();
}
fun inner() {
  var x = 1;
  return x + "!";
}
outer();
EOF
echo "\"$CLOX\" \"$TMP/trace.lox\"" > "$TMP/cmd"
check "stack trace lists every frame" 70 "" \
  "Operands must be two numbers or two strings.${nl}[line 7] in inner()${nl}[line 3] in middle()${nl}[line 1] in outer()${nl}[line 9] in script"

# A wrong argument count is reported before the callee starts, so the
# innermost frame in the trace is the caller.
cat > "$TMP/arity.lox" <<'EOF'
fun takesTwo(a, b) {}
fun caller() {
  takesTwo(1);
}
caller();
EOF
echo "\"$CLOX\" \"$TMP/arity.lox\"" > "$TMP/cmd"
check "arity error is reported in the caller" 70 "" \
  "Expected 2 arguments but got 1.${nl}[line 3] in caller()${nl}[line 5] in script"

# The 16-bit limit on forward jumps. Each "nil;" compiles to 2 bytes
# (OP_NIL, OP_POP), and the jump over the then-branch also crosses its OP_POP
# (1 byte) and the OP_JUMP over the else-branch (3 bytes): 2n + 4 bytes.
# These programs are ~130 KB, so they're generated here, not stored in tests/.
bigif() {
  awk -v n="$1" 'BEGIN { printf "if (false) {"; for (i = 0; i < n; i++) printf " nil;"; print " }"; print "print \"ok\";" }' > "$TMP/big.lox"
}
bigif 32765   # jump = 65534 bytes: fits in 16 bits
echo "\"$CLOX\" \"$TMP/big.lox\"" > "$TMP/cmd"
check "jump of 65534 bytes works" 0 "ok" ""
bigif 32766   # jump = 65536 bytes: too far
check "jump of 65536 bytes is a compile error" 65 "" \
  "[line 1] Error at '}': Too much code to jump over."

if [ "$failed" -ne 0 ]; then
  echo "$passed passed, $failed failed"
  exit 1
fi
echo "PASS ($passed checks)"
