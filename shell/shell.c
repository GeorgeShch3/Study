/* =========================================================================
   A simple command shell.

   Supports: commands with arguments, quotes, redirections < > >>, pipes |,
   logical operators && and ||, separators ; and background &, the built-in
   cd, and grouping with parentheses ( )

   How a line is processed (call tree, top to bottom):

     main
      reader              -> split the line into tokens (words + operators)
      executor
        parse_shell_command   -> split on ; and &
          parse_command       -> split on && and ||
            command           -> split on | ; a group (...) goes to run_subshell
              run_subshell           -> run a group in a fork
              redirection_inp_out    -> apply < > >>
                simple_command       -> fork + execvp (or cd)
              apply_redirections_and_exec  -> same, for one pipe stage
   ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define STR_CAP               10    /* initial array capacity              */
#define COMMAND_NOT_FOUND     1     /* exit code: execvp could not run cmd */
#define FAILD_ALLOCATE_MEMORY 2     /* exit code: memory allocation failed */
#define SYNTAX_ERROR          4     /* exit code: bad syntax               */
#define UNEXPECTED_ERROR      8     /* exit code: unexpected failure       */
#define EXIT_CMD_NOT_FOUND    127   /* bash status: command not found      */
#define EXIT_CMD_NOEXEC       126   /* bash status: found but not runnable */

/* tells check_syscall/warn_syscall which call failed, for the message */
enum syscall_op { OP_OPEN = 1, OP_CLOSE, OP_DUP };

struct strings
{
    char** strs;
    int    cap;
    int    size;
};

/* exit status of the last command (like $? in bash); a subshell reads it
   to report the status of its last command */
int last_status = 0;

int is_special(int c) {
    return c == '&' || c == '>' || c == '<' || c == '|'
        || c == '(' || c == ')' || c == ';';
}

/* Called in a child after execvp fails. Picks a message and exit code from
   errno (127 = not found, 126 = no permission). Does not return. */
void exec_failure_exit(const char* cmd) {
    int code;
    if (errno == ENOENT) {
        fprintf(stderr, "%s: command not found\n", cmd);
        code = EXIT_CMD_NOT_FOUND;
    } else if (errno == EACCES || errno == ENOEXEC) {
        fprintf(stderr, "%s: %s\n", cmd, strerror(errno));
        code = EXIT_CMD_NOEXEC;
    } else {
        fprintf(stderr, "%s: %s\n", cmd, strerror(errno));
        code = EXIT_CMD_NOT_FOUND;
    }
    for (int fd = 3; fd < 256; fd++) close(fd);
    fflush(NULL);
    _exit(code);
}

/* Builds a NULL-terminated argv from tokens [begin, end). The array points
   to the same strings, so the caller frees only the array, not the strings
   (they belong to struct strings). Returns NULL if malloc fails. */
char** build_argv(char** str, int begin, int end) {
    int n = end - begin;
    if (n < 0) n = 0;

    char** argv = (char**)malloc(sizeof(char*) * (n + 1));
    if (argv == NULL) {
        fprintf(stderr, "Unable to allocate memory for argv\n");
        return NULL;
    }
    for (int i = 0; i < n; i++)
        argv[i] = str[begin + i];
    argv[n] = NULL;
    return argv;
}

/* --- Dynamic token array (struct strings) -------------------------------
   add: append a copy of a string, growing the array if needed.
   initialize_strings / clear: create and free.
   capacity_up: grow a char buffer (used by reader).
   All of them stop the program if memory runs out.                       */

void add(struct strings* strs, char* str) {
    if (strs->cap == strs->size) {
        int old_cap = strs->cap;
        strs->cap *= 2;
        strs->strs = (char**)realloc(strs->strs, sizeof(char*) * strs->cap);
        if (strs->strs == NULL) {
            fprintf(stderr, "Unable to allocate memory realloc\n");
            exit(FAILD_ALLOCATE_MEMORY);
        }

        for (int i = old_cap; i < strs->cap; i++)
            strs->strs[i] = NULL;
    }

    strs->strs[strs->size++] = strdup(str);
    if (strs->strs[strs->size - 1] == NULL) {
        fprintf(stderr, "Unable to allocate memory strdup\n");
        exit(FAILD_ALLOCATE_MEMORY);
    }
}

void initialize_strings(struct strings* strs) {
    strs->cap  = STR_CAP;
    strs->size = 0;
    strs->strs = (char**)calloc(STR_CAP, sizeof(char*));

    if (strs->strs == NULL) {
        fprintf(stderr, "Unable to allocate memory calloc\n");
        exit(FAILD_ALLOCATE_MEMORY);
    }
}

void clear(struct strings* strs) {
    for (int i = 0; i < strs->size; i++)
        free(strs->strs[i]);
    free(strs->strs);

    strs->cap  = 0;
    strs->size = 0;
    strs->strs = NULL;
}

char* capacity_up(char* str, int* capacity, int size) {
    if (*capacity == size + 1) {
        *capacity *= 2;
        str = (char*)realloc(str, *capacity);
        if (str == NULL) {
            fprintf(stderr, "Unable to allocate memory realloc\n");
            exit(FAILD_ALLOCATE_MEMORY);
        }
    }
    return str;
}

/* Reads one line from stdin and splits it into tokens - words and operators
   (| & > < ; ( ) and the pairs &&, ||, >>), keeping quoted text together.
   Each token is added to strs. Returns 0 on a normal line, EOF at end of input. */
int reader(struct strings* strs) {
    int  c;
    int  counter  = 0;
    int  capacity = STR_CAP;

    char* str = (char*)malloc(sizeof(char) * STR_CAP);
    if (str == NULL) {
        fprintf(stderr, "Unable to allocate memory malloc\n");
        exit(FAILD_ALLOCATE_MEMORY);
    }

    /* states for reading one character at a time: START - waiting for a
       token, SPACE - skipping spaces, DQUOTES - inside quotes, SYMBOL -
       inside a normal word, SPECIAL_SYMBOL - an operator */
    enum STATES { START, SPACE, DQUOTES, SYMBOL, SPECIAL_SYMBOL } state = START;

    while ((c = getchar()) != '\n' && c != EOF) {
        switch (state) {
        case START:
            if (c == '"') {
                state = DQUOTES;
            } else if (is_special(c)) {
                state = SPECIAL_SYMBOL;
                ungetc(c, stdin);
            } else if (isspace(c)) {
                state = SPACE;
            } else {
                state = SYMBOL;
                ungetc(c, stdin);
            }
            if (counter) {
                str[counter] = '\0';
                counter = 0;
                add(strs, str);
            }
            break;

        case SPACE:
            if (!isspace(c)) {
                state = START;
                ungetc(c, stdin);
            }
            break;

        case DQUOTES:

            if (c != '"') {
                str = capacity_up(str, &capacity, counter);
                str[counter++] = c;
            } else {
                state = SYMBOL;
            }
            break;

        case SYMBOL:
            if (is_special(c) || isspace(c)) {
                ungetc(c, stdin);
                state = START;
            } else if (c == '"') {
                state = DQUOTES;
            } else {
                str = capacity_up(str, &capacity, counter);
                str[counter++] = c;
            }
            break;

        case SPECIAL_SYMBOL:
            str = capacity_up(str, &capacity, counter);
            str[counter++] = c;
            if (c == '&' || c == '>' || c == '|') {
                int n = getchar();
                if (n == c) {
                    str = capacity_up(str, &capacity, counter);
                    str[counter++] = n;
                } else if (n != EOF) {
                    ungetc(n, stdin);
                }
            }
            state = START;
            break;
        }
    }

    if (counter) {
        str[counter] = '\0';
        add(strs, str);
    }
    free(str);

    return (c == EOF) ? EOF : 0;
}

/* Checks that a redirection operator (> >> <) is followed by a file name,
   not another operator or the end of the line. Stops the program on error. */
void check_conditions(char** str, int begin, int end) {
    if (begin + 1 == end) {
        fprintf(stderr, "bash: syntax error near unexpected token 'newline'\n");
        exit(SYNTAX_ERROR);
    }
    if ((!strcmp(str[begin], ">") || !strcmp(str[begin], ">>") || !strcmp(str[begin], "<")) &&
        (!strcmp(str[begin + 1], "(")  || !strcmp(str[begin + 1], ")")  || !strcmp(str[begin + 1], "|")
      || !strcmp(str[begin + 1], "||") || !strcmp(str[begin + 1], "&")  || !strcmp(str[begin + 1], "&&")
      || !strcmp(str[begin + 1], ";")  || !strcmp(str[begin + 1], ">")  || !strcmp(str[begin + 1], ">>")
      || !strcmp(str[begin + 1], "<"))) {
        fprintf(stderr, "bash: syntax error near unexpected token '%s'\n", str[begin + 1]);
        exit(SYNTAX_ERROR);
    }
}

/* The cd built-in. With no argument it goes to $HOME. Returns 0 on success,
   1 on error (so the status can go into $?). */
int builtin_cd(char** str, int begin, int end) {
    const char* target;

    if (begin + 2 < end) {
        fprintf(stderr, "cd: too many arguments\n");
        return 1;
    }

    if (begin + 1 < end && str[begin + 1] != NULL) {
        target = str[begin + 1];
    } else {
        target = getenv("HOME");
        if (target == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
    }

    if (chdir(target) != 0) {
        fprintf(stderr, "cd: %s: %s\n", target, strerror(errno));
        return 1;
    }
    return 0;
}

/* Runs one command from tokens [begin, end). cd runs in the shell itself;
   everything else runs through fork + execvp. With background != 0 it does
   not wait. Returns the command's exit code. */
int simple_command(char** str, int begin, int end, int background) {
    pid_t pid;
    int   status = 0;
    int   ret    = 0;

    if (!strcmp(str[begin], "cd"))
        return builtin_cd(str, begin, end);

    char** argv = build_argv(str, begin, end);
    if (argv == NULL)
        return 1;

    if ((pid = fork()) < 0) {

        fprintf(stderr, "fork: %s\n", strerror(errno));
        ret = 1;
    } else if (pid == 0) {
        execvp(argv[0], argv);
        exec_failure_exit(argv[0]);
    } else if (!background) {

        while (waitpid(pid, &status, 0) < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "waitpid: %s\n", strerror(errno));
            break;
        }
        ret = WEXITSTATUS(status);
    } else {
        printf("[background pid %d]\n", pid);
    }

    free(argv);
    return ret;
}

/* If err < 0 (the syscall op failed), print a message.
   check_syscall is for critical spots: it stops the program.
   warn_syscall is for recoverable spots (restoring our streams): it only
   reports, so one failure does not kill the whole shell session. */
void check_syscall(int err, enum syscall_op op) {
    if (err < 0) {
        switch (op) {
        case OP_OPEN:  fprintf(stderr, "open: %s\n",  strerror(errno)); break;
        case OP_CLOSE: fprintf(stderr, "close: %s\n", strerror(errno)); break;
        case OP_DUP:   fprintf(stderr, "dup: %s\n",   strerror(errno)); break;
        }
        exit(UNEXPECTED_ERROR);
    }
}

void warn_syscall(int err, enum syscall_op op) {
    if (err < 0) {
        switch (op) {
        case OP_OPEN:  fprintf(stderr, "open: %s\n",  strerror(errno)); break;
        case OP_CLOSE: fprintf(stderr, "close: %s\n", strerror(errno)); break;
        case OP_DUP:   fprintf(stderr, "dup: %s\n",   strerror(errno)); break;
        }
    }
}

/* Applies redirections < > >> from tokens [begin, end): opens the files and
   swaps stdin/stdout with dup2 after saving the originals, runs the command,
   then puts the streams back. Returns the command's exit code (or 1 if a
   redirection file could not be opened). */
int redirection_inp_out(char** str, int begin, int end, int background) {
    int   file;
    int   result    = 0;
    int   new_end   = begin;
    int   cmd_begin = begin;
    int   cmd_end   = end;
    char* word      = NULL;

    int   stdin_backup  = dup(0);
    int   stdout_backup = dup(1);
    int   failed        = 0;
    check_syscall(stdin_backup, OP_DUP);
    check_syscall(stdout_backup, OP_DUP);

    fcntl(stdin_backup,  F_SETFD, FD_CLOEXEC);
    fcntl(stdout_backup, F_SETFD, FD_CLOEXEC);

    while (new_end < end && !failed) {
        word = str[new_end++];

        if (!strcmp(word, ">") || !strcmp(word, ">>") || !strcmp(word, "<")) {
            check_conditions(str, new_end - 1, end);

            if (!strcmp(word, ">"))
                file = open(str[new_end], O_CREAT | O_WRONLY | O_TRUNC, 0666);
            else if (!strcmp(word, ">>"))
                file = open(str[new_end], O_CREAT | O_WRONLY | O_APPEND, 0666);
            else
                file = open(str[new_end], O_RDONLY);

            if (file < 0) {
                fprintf(stderr, "%s: %s\n", str[new_end], strerror(errno));
                result = 1;
                failed = 1;
                break;
            }

            if (dup2(file, !strcmp(word, "<") ? 0 : 1) < 0) {
                fprintf(stderr, "dup2: %s\n", strerror(errno));
                close(file);
                result = 1;
                failed = 1;
                break;
            }
            warn_syscall(close(file), OP_CLOSE);
            cmd_begin = ++new_end;

            if (cmd_end == end)
                cmd_end = new_end - 1;
        } else {
            cmd_end = new_end;
        }
    }

    if (!failed) {
        if (cmd_begin == end)
            result = simple_command(str, begin, cmd_end, background);
        else
            result = simple_command(str, cmd_begin, end, background);
    }

    warn_syscall(dup2(stdin_backup, 0),  OP_DUP);
    warn_syscall(dup2(stdout_backup, 1), OP_DUP);

    close(stdin_backup);
    close(stdout_backup);

    return result;
}

/* Reaps finished background jobs without blocking and prints their status. */
void background_process_check(void) {
    int   status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status))
            printf("[%d] exited; status = %d\n", pid, WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
            printf("[%d] killed by signal %d\n", pid, WTERMSIG(status));
        else if (WIFSTOPPED(status))
            printf("[%d] stopped by signal %d\n", pid, WSTOPSIG(status));
        else if (WIFCONTINUED(status))
            printf("[%d] continued\n", pid);
    }
}

/* Like redirection_inp_out, but for one pipe stage: it runs in a child that
   is already forked, applies < > >>, and then calls execvp. Does not return. */
void apply_redirections_and_exec(char** str, int begin, int end) {
    int   file;
    int   new_end   = begin;
    int   cmd_begin = begin;
    int   cmd_end   = end;
    char* word      = NULL;

    while (new_end < end) {
        word = str[new_end++];

        if (!strcmp(word, ">") || !strcmp(word, ">>") || !strcmp(word, "<")) {
            check_conditions(str, new_end - 1, end);

            if (!strcmp(word, ">"))
                file = open(str[new_end], O_CREAT | O_WRONLY | O_TRUNC, 0666);
            else if (!strcmp(word, ">>"))
                file = open(str[new_end], O_CREAT | O_WRONLY | O_APPEND, 0666);
            else
                file = open(str[new_end], O_RDONLY);

            if (file < 0) {
                fprintf(stderr, "%s: %s\n", str[new_end], strerror(errno));
                fflush(NULL);
                _exit(1);
            }

            check_syscall(dup2(file, !strcmp(word, "<") ? 0 : 1), OP_DUP);
            close(file);
            cmd_begin = ++new_end;

            if (cmd_end == end)
                cmd_end = new_end - 1;
        } else {
            cmd_end = new_end;
        }
    }

    if (cmd_begin != end)
        begin = cmd_begin;

    char** argv = build_argv(str, begin, cmd_end);
    if (argv == NULL) _exit(1);

    execvp(argv[0], argv);
    exec_failure_exit(argv[0]);
}

/* Returns the index of the ')' matching the '(' at open, or -1 if none. */
int match_paren(char** str, int open, int end) {
    int depth = 0;
    for (int i = open; i < end; i++) {
        if (!strcmp(str[i], "(")) depth++;
        else if (!strcmp(str[i], ")")) {
            depth--;
            if (depth == 0) return i;
        }
    }
    return -1;
}

int command(char** str, int begin, int end, int background);   /* forward decls */
void parse_shell_command(char** str, int size);

/* Runs a group ( ... ) as a subshell: forks, and the child runs the tokens
   inside the parens through the grammar again. cd/env changes stay in the
   child (isolation). Redirections after ')' apply to the whole group.
   Returns the group's exit code. */
int run_subshell(char** str, int begin, int end, int background) {
    int close_idx = match_paren(str, begin, end);
    if (close_idx < 0) {
        fprintf(stderr, "bash: syntax error near unexpected token '('\n");
        return 2;
    }

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "fork: %s\n", strerror(errno));
        return 1;
    }

    if (pid == 0) {

        int new_end = close_idx + 1;
        while (new_end < end) {
            char* w = str[new_end++];
            if (!strcmp(w, ">") || !strcmp(w, ">>") || !strcmp(w, "<")) {
                int file;
                if (!strcmp(w, ">"))
                    file = open(str[new_end], O_CREAT | O_WRONLY | O_TRUNC, 0666);
                else if (!strcmp(w, ">>"))
                    file = open(str[new_end], O_CREAT | O_WRONLY | O_APPEND, 0666);
                else
                    file = open(str[new_end], O_RDONLY);

                if (file < 0) {
                    fprintf(stderr, "%s: %s\n", str[new_end], strerror(errno));
                    fflush(NULL);
                    _exit(1);
                }
                dup2(file, !strcmp(w, "<") ? 0 : 1);
                close(file);
                new_end++;
            }
        }

        char* saved = str[close_idx];
        str[close_idx] = NULL;
        last_status = 0;
        parse_shell_command(str + begin + 1, close_idx - (begin + 1));
        str[close_idx] = saved;

        fflush(NULL);
        _exit(last_status);
    }

    if (background) {
        printf("[background pid %d]\n", pid);
        return 0;
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
        ;
    return WEXITSTATUS(status);
}

/* Pipeline level. A whole group (...) goes to run_subshell. If there is a |
   (outside parens), it builds a pipeline: one fork per stage joined by pipes,
   then waits for all. Otherwise it is a single command via redirection_inp_out.
   Returns the exit code (for a pipeline, the last stage's). */
int command(char** str, int begin, int end, int background) {

    int has_pipe = 0;
    int pdepth   = 0;
    for (int i = begin; i < end; i++) {
        if (!strcmp(str[i], "(")) pdepth++;
        else if (!strcmp(str[i], ")")) pdepth--;
        else if (pdepth == 0 && !strcmp(str[i], "|")) { has_pipe = 1; break; }
    }

    if (!has_pipe && begin < end && !strcmp(str[begin], "("))
        return run_subshell(str, begin, end, background);

    if (!has_pipe)
        return redirection_inp_out(str, begin, end, background);

    int   fd[2];
    int   new_begin = begin;
    int   new_end   = begin;
    char* word      = NULL;

    int saved_stdin  = dup(STDIN_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);
    check_syscall(saved_stdin, OP_DUP);
    check_syscall(saved_stdout, OP_DUP);
    fcntl(saved_stdin,  F_SETFD, FD_CLOEXEC);
    fcntl(saved_stdout, F_SETFD, FD_CLOEXEC);

    int   pid_counter = 0;
    pid_t last_pid    = -1;
    int   sdepth      = 0;
    int   aborted     = 0;

    while (new_end < end && !aborted) {
        word = str[new_end++];

        if (!strcmp(word, "(")) sdepth++;
        else if (!strcmp(word, ")")) sdepth--;

        int at_pipe = (sdepth == 0 && !strcmp(word, "|"));

        if (at_pipe || new_end == end) {
            int is_pipe   = at_pipe;
            int stage_end = is_pipe ? (new_end - 1) : new_end;

            if (is_pipe && pipe(fd) < 0) {

                fprintf(stderr, "pipe: %s\n", strerror(errno));
                aborted = 1;
                break;
            }

            pid_t pid = fork();
            if (pid < 0) {
                fprintf(stderr, "fork: %s\n", strerror(errno));
                if (is_pipe) { close(fd[0]); close(fd[1]); }
                aborted = 1;
                break;
            } else if (pid == 0) {
                if (is_pipe) {
                    if (dup2(fd[1], 1) < 0) {
                        fprintf(stderr, "dup2: %s\n", strerror(errno));
                        _exit(1);
                    }
                    close(fd[0]); close(fd[1]);
                }

                if (!strcmp(str[new_begin], "("))
                    _exit(run_subshell(str, new_begin, stage_end, 0));
                apply_redirections_and_exec(str, new_begin, stage_end);
            }
            last_pid = pid;
            pid_counter++;

            if (is_pipe) {
                new_begin = new_end;
                if (dup2(fd[0], 0) < 0)
                    fprintf(stderr, "dup2: %s\n", strerror(errno));
                close(fd[0]); close(fd[1]);
            }
        }
    }

    warn_syscall(dup2(saved_stdin,  STDIN_FILENO),  OP_DUP);
    warn_syscall(dup2(saved_stdout, STDOUT_FILENO), OP_DUP);
    close(saved_stdin); close(saved_stdout);

    int status = 0;
    if (!background) {
        for (int i = 0; i < pid_counter; i++) {
            pid_t target = (last_pid > 0 && i == pid_counter - 1) ? last_pid : -1;
            int   st;
            pid_t w;
            while ((w = waitpid(target, &st, 0)) < 0 && errno == EINTR)
                ;
            if (w == last_pid) status = st;
        }
    }

    return WEXITSTATUS(status);
}

/* The && / || level (left to right, operators outside parens). For each
   segment it decides from the previous status whether to run it (&& on
   success, || on failure) and passes it to command. Updates last_status. */
void parse_command(char** str, int begin, int end, int background) {
    int   seg_begin = begin;
    int   new_end   = begin;
    int   run_next  = 1;
    int   status    = 0;
    int   depth     = 0;

    while (new_end < end) {
        char* word = str[new_end];

        if (!strcmp(word, "(")) depth++;
        else if (!strcmp(word, ")")) depth--;
        else if (depth == 0 && (!strcmp(word, "&&") || !strcmp(word, "||"))) {
            if (run_next) {
                status = command(str, seg_begin, new_end, background);
                last_status = status;
            }

            if (!strcmp(word, "&&"))
                run_next = (status == 0);
            else
                run_next = (status != 0);

            seg_begin = new_end + 1;
        }
        new_end++;
    }

    if (run_next && seg_begin < end)
        last_status = command(str, seg_begin, end, background);
}

/* Top level: splits the line on ; and & (outside parens). A segment before &
   runs in the background; the rest run normally. Each goes to parse_command. */
void parse_shell_command(char** str, int size) {
    int   begin = 0;
    int   end   = 0;
    int   depth = 0;
    char* word  = NULL;

    while (end < size) {
        word = str[end++];

        if (!strcmp(word, "(")) { depth++; continue; }
        if (!strcmp(word, ")")) { depth--; continue; }
        if (depth > 0) continue;

        if (!strcmp(word, ";")) {
            parse_command(str, begin, end - 1, 0);
            begin = end;
        }
        if (!strcmp(word, "&")) {

            parse_command(str, begin, end - 1, 1);
            begin = end;
        }
    }

    if (begin < size)
        parse_command(str, begin, size, 0);
}

void print_prompt(void) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
        printf("%s$ ", cwd);
    else
        printf("$ ");
    fflush(stdout);
}

/* Checks paren balance before running. Returns 1 if parens are balanced and
   there is no empty group (), else 0. */
int parens_ok(char** str, int size) {
    int depth = 0;
    for (int i = 0; i < size; i++) {
        if (!strcmp(str[i], "(")) {
            depth++;
        } else if (!strcmp(str[i], ")")) {
            depth--;
            if (depth < 0) return 0;
            if (i > 0 && !strcmp(str[i - 1], "("))
                return 0;
        }
    }
    return depth == 0;
}

/* Runs one parsed line: check paren balance, parse and run it, reap
   background jobs, and print the next prompt. */
void executor(struct strings* strs) {
    if (!parens_ok(strs->strs, strs->size)) {
        fprintf(stderr, "bash: syntax error near unexpected token\n");
    } else {
        parse_shell_command(strs->strs, strs->size);
    }
    background_process_check();
    print_prompt();
}

/* Main loop: print the prompt, read a line, run it, clear the tokens, and
   repeat until end of input (EOF / Ctrl+D). */
int main(int argc, char** argv) {
    (void)argc; (void)argv;

    struct strings words;
    initialize_strings(&words);

    print_prompt();

    while (1) {
        int eof = reader(&words);

        if (eof == EOF && words.size == 0) {
            printf("\n");
            break;
        }

        executor(&words);
        clear(&words);
        initialize_strings(&words);

        if (eof == EOF) {
            printf("\n");
            break;
        }
    }

    clear(&words);
    return 0;
}
