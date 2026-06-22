#!/usr/bin/env bash
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INTERP="${1:-$HERE/../interpreter}"

if [ ! -x "$INTERP" ]; then
    echo "Interpreter not found or not executable: $INTERP"
    echo "Build it first, e.g.:  g++ -std=c++17 -O2 -o interpreter interpreter.cpp"
    exit 2
fi

pass=0
fail=0
failed_names=()

green() { printf '\033[32m%s\033[0m' "$1"; }
red()   { printf '\033[31m%s\033[0m' "$1"; }

extract_output() {
    sed -n '/^Success!$/,/^Finish of executing\.$/p' \
        | sed '1d;$d' \
        | grep -v '^Input .* value for '
}

echo "=== A-tests (valid programs) ==="
for prog in "$HERE"/a_tests/*.txt; do
    name="$(basename "$prog" .txt)"
    base="${prog%.txt}"
    expected="$base.expected"
    input="$base.input"
    [ -f "$expected" ] || { echo "  ?  $name (no .expected)"; continue; }

    if [ -f "$input" ]; then
        raw="$("$INTERP" "$prog" < "$input" 2>&1)"
    else
        raw="$("$INTERP" "$prog" < /dev/null 2>&1)"
    fi
    got="$(printf '%s\n' "$raw" | extract_output)"
    want="$(cat "$expected")"

    if [ "$got" == "$want" ]; then
        echo "  $(green PASS)  $name"
        pass=$((pass + 1))
    else
        echo "  $(red FAIL)  $name"
        echo "    expected: $(printf '%s' "$want" | tr '\n' '|')"
        echo "    got:      $(printf '%s' "$got" | tr '\n' '|')"
        fail=$((fail + 1))
        failed_names+=("$name")
    fi
done

echo
echo "=== B-tests (error programs) ==="
for prog in "$HERE"/b_tests/*.txt; do
    name="$(basename "$prog" .txt)"
    base="${prog%.txt}"
    expect="$base.expect_error"
    [ -f "$expect" ] || { echo "  ?  $name (no .expect_error)"; continue; }

    raw="$("$INTERP" "$prog" < /dev/null 2>&1)"
    needle="$(cat "$expect")"

    if printf '%s' "$raw" | grep -qiF "$needle"; then
        echo "  $(green PASS)  $name  (matched: \"$needle\")"
        pass=$((pass + 1))
    else
        echo "  $(red FAIL)  $name"
        echo "    wanted error containing: $needle"
        echo "    got: $(printf '%s' "$raw" | grep -i error | head -1)"
        fail=$((fail + 1))
        failed_names+=("$name")
    fi
done

echo
echo "=================================="
echo "Total: $((pass + fail))   Passed: $pass   Failed: $fail"
if [ "$fail" -ne 0 ]; then
    echo "Failed: ${failed_names[*]}"
    exit 1
fi
echo "All tests passed."