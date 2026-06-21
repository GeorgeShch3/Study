#!/usr/bin/env bash

SHELL_BIN="${1:-./myshell}"
case "$SHELL_BIN" in /*) : ;; *) SHELL_BIN="$(pwd)/$SHELL_BIN" ;; esac

if [ ! -x "$SHELL_BIN" ]; then
    echo "Binary '$SHELL_BIN' not found. Compile first:"
    echo "  gcc -Wall -Wextra -O2 -o myshell myshell.c"
    exit 1
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

cat > "$WORK/failsys.c" << 'EOF'
#define _GNU_SOURCE
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <dlfcn.h>
static int on(const char* v){ const char* s=getenv(v); return s && s[0]=='1'; }
pid_t fork(void){
    if(on("FAIL_FORK")){ errno=EAGAIN; return -1; }
    static pid_t(*real)(void)=0; if(!real) real=dlsym(RTLD_NEXT,"fork");
    return real();
}
int pipe(int fd[2]){
    if(on("FAIL_PIPE")){ errno=EMFILE; return -1; }
    static int(*real)(int[2])=0; if(!real) real=dlsym(RTLD_NEXT,"pipe");
    return real(fd);
}
int dup2(int a,int b){
    if(on("FAIL_DUP2")){ errno=EBADF; return -1; }
    static int(*real)(int,int)=0; if(!real) real=dlsym(RTLD_NEXT,"dup2");
    return real(a,b);
}
EOF
gcc -shared -fPIC -o "$WORK/failsys.so" "$WORK/failsys.c" -ldl || {
    echo "could not build shim"; exit 1; }

PASS=0
FAIL=0

fault_test() {
    local name="$1" env="$2" input="$3" marker="$4"
    local out rc
    out="$(printf '%b' "$input" | timeout 10 \
           env "$env" LD_PRELOAD="$WORK/failsys.so" "$SHELL_BIN" 2>&1)"
    rc=$?

    local ok=1
    if [ -n "$marker" ]; then
        echo "$out" | grep -qF -- "$marker" || ok=0
    else
        local prompts
        prompts="$(echo "$out" | grep -c '\$ ')"
        [ "$prompts" -ge 2 ] || ok=0
    fi
    [ "$rc" -ge 124 ] && ok=0

    if [ "$ok" = "1" ]; then
        printf '  \033[32mPASS\033[0m  %s\n' "$name"
        PASS=$((PASS+1))
    else
        printf '  \033[31mFAIL\033[0m  %s (rc=%s)\n' "$name" "$rc"
        printf '        output: %s\n' "$(echo "$out" | tr '\n' '|')"
        FAIL=$((FAIL+1))
    fi
}

echo "=== Fault injection: shell must survive syscall failures ==="
fault_test "fork fails (simple cmd)"  "FAIL_FORK=1" \
    'ls\nls\n'                         ""
fault_test "fork fails (pipeline)"    "FAIL_FORK=1" \
    'ls | wc -l\nls | wc -l\n'         ""
fault_test "pipe fails"               "FAIL_PIPE=1" \
    'ls | wc -l\necho SURVIVED\n'      "SURVIVED"
fault_test "dup2 fails (pipeline)"    "FAIL_DUP2=1" \
    'echo hi | cat\necho SURVIVED\n'   "SURVIVED"
fault_test "dup2 fails (redirect)"    "FAIL_DUP2=1" \
    'echo hi > f.txt\necho SURVIVED\n' "SURVIVED"

echo
echo "----------------------------------------"
printf 'Total: %d  \033[32mPASS: %d\033[0m  \033[31mFAIL: %d\033[0m\n' \
    "$((PASS+FAIL))" "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ] && echo "Shell survives all injected faults." \
                  || echo "Shell crashed/hung on some faults — see above."
exit "$FAIL"