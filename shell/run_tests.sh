#!/usr/bin/env bash

SHELL_BIN="${1:-./myshell}"

if [ ! -x "$SHELL_BIN" ]; then
    echo "Binary '$SHELL_BIN' not found or not executable."
    echo "Compile first:  gcc -Wall -Wextra -O2 -o myshell myshell.c"
    exit 1
fi

case "$SHELL_BIN" in
    /*) : ;;
    *)  SHELL_BIN="$(pwd)/$SHELL_BIN" ;;
esac

PASS=0
FAIL=0
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

run_test() {
    local name="$1" input="$2" mode="$3" expected="$4"
    local out
    out="$(printf '%b' "$input" | "$SHELL_BIN" 2>&1)"

    local ok=0
    if [ "$mode" = "has" ]; then
        echo "$out" | grep -qF -- "$expected" && ok=1
    else
        echo "$out" | grep -qF -- "$expected" || ok=1
    fi

    if [ "$ok" = "1" ]; then
        printf '  \033[32mPASS\033[0m  %s\n' "$name"
        PASS=$((PASS+1))
    else
        printf '  \033[31mFAIL\033[0m  %s\n' "$name"
        printf '        expected (%s): %s\n' "$mode" "$expected"
        printf '        got: %s\n' "$(echo "$out" | tr '\n' '|')"
        FAIL=$((FAIL+1))
    fi
}

cd "$WORK" || exit 1

echo "=== Basic commands & quoting ==="
run_test "echo simple"            'echo hello world\n'        has   "hello world"
run_test "quotes keep spaces"     'echo "a   b"\n'            has   "a   b"
run_test "quote glued to word"    'echo "foo"bar\n'          has   "foobar"
run_test "multi quote glue"       'echo a"b"c"d"\n'           has   "abcd"

echo "=== Redirections ==="
run_test "> creates"              'echo first > f.txt\ncat f.txt\n'                  has  "first"
run_test "> overwrites"           'echo OLDLINE > f.txt\necho NEWLINE > f.txt\ncat f.txt\n'  hasnt "OLDLINE"
run_test ">> appends"             'echo a > f.txt\necho b >> f.txt\ncat f.txt\n'     has  "b"
run_test ">> keeps old line"      'echo a > f.txt\necho b >> f.txt\ncat f.txt\n'     has  "a"
run_test "< input redirect"       'echo zzz > f.txt\ncat < f.txt\n'                  has  "zzz"
run_test "restore after redirect" 'echo x > f.txt\necho VISIBLE\n'                   has  "VISIBLE"

echo "=== Pipes ==="
run_test "simple pipe"            'echo hi | tr a-z A-Z\n'                           has  "HI"
run_test "three-stage pipe"       'printf "c\\nb\\na\\n" | sort | uniq\n'           has  "a"
run_test "pipe + > redirect"      'echo hey | tr a-z A-Z > p.txt\ncat p.txt\n'      has  "HEY"
run_test "< + pipe + >"           'echo data > i.txt\ncat < i.txt | sort > o.txt\ncat o.txt\n' has "data"

echo "=== && and || ==="
run_test "true &&"                'true && echo YES\n'                              has   "YES"
run_test "false && skips"         'false && echo NOPE_MARK\n'                            hasnt "NOPE_MARK"
run_test "false ||"               'false || echo FB\n'                              has   "FB"
run_test "true || skips"          'true || echo SKIP_MARK\n'                             hasnt "SKIP_MARK"
run_test "chain && &&"            'true && echo A && echo B\n'                      has   "B"
run_test "false && _ || _"        'false && echo A || echo B\n'                     has   "B"
run_test "true && _ || _ "        'true && echo XX_MARK || echo YY_MARK\n'                      hasnt "YY_MARK"
run_test "false || _ && _"        'false || echo P && echo Q\n'                     has   "Q"
run_test "short-circuit ||"       'true || echo skip_mark && echo run_mark\n'                 hasnt "skip_mark"
run_test "short-circuit run"      'true || echo skip_mark && echo run_mark\n'                 has   "run_mark"

echo "=== Semicolon ==="
run_test "semicolon sequence"     'echo one ; echo two ; echo three\n'              has   "three"

echo "=== Builtin cd ==="
run_test "cd changes dir"         'cd /tmp\npwd\n'                                  has   "/tmp"
run_test "cd bad dir errors"      'cd /no_such_dir_42\n'                            has   "cd:"

echo "=== Background ==="
run_test "bg announces pid"       'sleep 1 &\necho immediate\n'                     has   "background pid"
run_test "bg non-blocking"        'sleep 1 &\necho immediate\n'                     has   "immediate"

echo "=== Subshells / parentheses ==="
run_test "group runs both"        '(echo GA; echo GB)\n'                              has   "GB"
run_test "semicolon inside group" '(echo GA; echo GB)\n'                              has   "GA"
run_test "cd in subshell, parent unaffected" 'cd /\n(cd /usr)\nls\n'                  has   "boot"
run_test "group | sort"           '(echo cc; echo aa; echo bb) | sort\n'             has   "aa"
run_test "command | group"        'echo PIPED | (cat; echo TAIL)\n'                   has   "TAIL"
run_test "group passes pipe data" 'echo PIPED | (cat; echo TAIL)\n'                   has   "PIPED"
run_test "group in &&"            'true && (echo GRP_AND)\n'                          has   "GRP_AND"
run_test "group in ||"            'false || (echo GRP_OR)\n'                          has   "GRP_OR"
run_test "nested groups"          '((echo NESTED))\n'                                 has   "NESTED"
run_test "subshell status false"  '(false) && echo NOT_THIS\n'                        hasnt "NOT_THIS"
run_test "subshell status true"   '(true) && echo YES_THIS\n'                         has   "YES_THIS"
run_test "group redirect"         '(echo R1; echo R2) > pg.txt\ncat pg.txt\n'         has   "R2"
run_test "unbalanced open paren"  '(echo BROKEN\n'                                    has   "syntax error"
run_test "unbalanced is skipped"  '(echo BROKEN\n'                                    hasnt "BROKEN"
run_test "empty group error"      '()\n'                                              has   "syntax error"
run_test "shell survives paren err" '(echo BROKEN\necho STILL_ALIVE\n'                has   "STILL_ALIVE"

echo "=== Errors don't crash shell ==="
run_test "unknown command"        'nosuchcmd_123\necho ALIVE\n'                     has   "command not found"
run_test "shell survives error"   'nosuchcmd_123\necho ALIVE\n'                     has   "ALIVE"
run_test "syntax error on >"      'echo hi >\n'                                     has   "syntax error"

echo
echo "----------------------------------------"
printf 'Total: %d  \033[32mPASS: %d\033[0m  \033[31mFAIL: %d\033[0m\n' \
    "$((PASS+FAIL))" "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ] && echo "All good." || echo "Some tests failed — see above."
exit "$FAIL"