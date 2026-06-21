#!/usr/bin/env bash

SRC="${1:-shell.c}"
BIN="myshell_vg"

if ! command -v valgrind >/dev/null 2>&1; then
    echo "valgrind not installed; skipping (install with: apt-get install valgrind)"
    exit 0
fi

gcc -Wall -Wextra -g -O0 -o "$BIN" "$SRC" || { echo "compile failed"; exit 1; }

SESSION='echo hello world
echo a > vg_f.txt
cat vg_f.txt
echo b >> vg_f.txt
cat < vg_f.txt
ls | wc -l
echo hi | tr a-z A-Z
printf "c\nb\na\n" | sort | uniq
cat < vg_f.txt | sort > vg_g.txt
cat vg_g.txt
(echo x; echo y) | sort
echo nested | (cat; echo done)
true && echo A || echo B
cat < /nonexistent_xyz
nosuchcmd_999
cd /tmp
pwd
'

OUT="$(mktemp)"
printf '%s' "$SESSION" | valgrind \
    --leak-check=full \
    --error-exitcode=42 \
    --track-fds=yes \
    --child-silent-after-fork=yes \
    ./"$BIN" > "$OUT" 2>&1
VG_RC=$?

rm -f vg_f.txt vg_g.txt

echo "--- valgrind summary (main process) ---"
grep -E "ERROR SUMMARY|FILE DESCRIPTORS|definitely lost|indirectly lost|All heap blocks|in use at exit" "$OUT"

FAIL=0
grep -q "All heap blocks were freed -- no leaks are possible" "$OUT" || {
    grep -q "definitely lost: 0 bytes" "$OUT" || { echo "LEAK: definitely lost > 0"; FAIL=1; }
}
grep -q "ERROR SUMMARY: 0 errors" "$OUT" || { echo "valgrind reported errors"; FAIL=1; }
grep -q "FILE DESCRIPTORS: 3 open (3 std) at exit" "$OUT" || {
    echo "FD LEAK: more than the 3 standard descriptors open at exit"; FAIL=1;
}
[ "$VG_RC" -eq 42 ] && { echo "valgrind error-exitcode tripped"; FAIL=1; }

rm -f "$OUT" "$BIN"

echo "---------------------------------------"
if [ "$FAIL" -eq 0 ]; then
    echo "VALGRIND: clean (no leaks, no fd leaks, no errors)"
else
    echo "VALGRIND: problems found (see above)"
fi
exit "$FAIL"