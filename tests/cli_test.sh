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
printf '1 + 2 * 3\n(1 + 2\n-4 / 2\n' > "$TMP/input"
echo "\"$CLOX\" < \"$TMP/input\"" > "$TMP/cmd"
check "repl evaluates lines and survives errors" 0 \
  "> 7${nl}> > -2${nl}> " \
  "[line 2] Error at end: Expect ')' after expression."

# Empty input: just the prompt, then the newline printed on EOF.
echo "\"$CLOX\" < /dev/null" > "$TMP/cmd"
check "repl exits cleanly on immediate EOF" 0 "> " ""

# Running a file.
printf '(1 + 2) * 3\n' > "$TMP/ok.lox"
echo "\"$CLOX\" \"$TMP/ok.lox\"" > "$TMP/cmd"
check "runs a file" 0 "9" ""

# Compile error in a file: exit 65.
printf '1 +' > "$TMP/bad.lox"
echo "\"$CLOX\" \"$TMP/bad.lox\"" > "$TMP/cmd"
check "compile error exits 65" 65 "" "[line 1] Error at end: Expect expression."

# Missing file: exit 74.
echo "\"$CLOX\" \"$TMP/does-not-exist.lox\"" > "$TMP/cmd"
check "missing file exits 74" 74 "" "Could not open file \"$TMP/does-not-exist.lox\"."

# Too many arguments: usage message, exit 64.
echo "\"$CLOX\" a b" > "$TMP/cmd"
check "too many arguments exits 64" 64 "" "Usage: clox [path]"

if [ "$failed" -ne 0 ]; then
  echo "$passed passed, $failed failed"
  exit 1
fi
echo "PASS ($passed checks)"
