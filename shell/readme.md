# myshell

A small command shell written in C.

## Features

- Run commands with arguments (`ls -la`, `echo hello`)
- Quoting: `echo "two   words"` keeps the spaces
- Redirections: `<` (input), `>` (overwrite), `>>` (append)
- Pipes: `ls | grep .c | wc -l`
- Logical operators: `&&` (run next on success), `||` (run next on failure)
- Sequencing: `;` runs commands one after another
- Background jobs: `sleep 5 &` returns the prompt immediately
- Built-in `cd` (changes the shell's own directory)
- Subshells with `( )`: `(cd /tmp; pwd)` runs in a fork, so it does not
  change the current directory of the shell. Groups can be piped,
  redirected, and nested: `(echo b; echo a) | sort`

The shell keeps running after errors: a missing command, a bad redirect
file, or a failed system call prints a message but does not crash the session.

## Build

```sh
gcc -Wall -Wextra -O2 -o myshell myshell.c
```

Run it:

```sh
./myshell
```

Type commands at the prompt. Exit with `Ctrl+D` (end of input).

## How it works

A line is processed in layers, each one splitting on a different operator:

```
reader              -> turn the line into tokens (words + operators)
parse_shell_command -> split on ; and &
  parse_command     -> split on && and ||
    command         -> split on | ; a group (...) runs as a subshell
      redirection_inp_out -> apply < > >>
        simple_command    -> fork + execvp (or run cd)
```

There is a longer explanation of the call tree at the top of `myshell.c`.

## Tests

The project comes with three test scripts.

### Functional tests

```sh
bash run_tests.sh
```

48 tests covering every feature: commands, quoting, redirections, pipes,
`&&`/`||`, `;`, `cd`, background jobs, subshells, and syntax errors. Each
test feeds a line to the shell and checks the output. Prints `PASS`/`FAIL`
per test and a total at the end.

### Fault-injection tests

```sh
bash run_fault_tests.sh
```

Checks that the shell survives when a system call fails. Real calls like
`fork`, `pipe`, and `dup2` almost never fail on their own, so these tests
use a small `LD_PRELOAD` library to force them to fail on purpose, then
verify the shell reports the error and keeps running instead of crashing
or hanging.

### Memory check (valgrind)

```sh
bash run_valgrind.sh
```

Runs a scripted session under valgrind and fails if there is any memory
leak, file-descriptor leak, or error in the main shell process. Requires
`valgrind` to be installed.

## Notes / limitations

- A lone `|` is ignored instead of being a syntax error (bash reports one).
- No environment variables (`$VAR`), globbing (`*`), or `~` expansion.
- No job control (`jobs`, `fg`, `bg`); background jobs are only reaped and
  reported before the next prompt.
