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

#define STR_CAP               10
#define COMMAND_NOT_FOUND     1
#define FAILD_ALLOCATE_MEMORY 2
#define FAILD_UNGETC          3
#define SYNTAX_ERROR          4
#define CLOSE_ERROR           5
#define OPEN_ERROR            6
#define DUP_ERROR             7
#define UNEXPECTED_ERROR      8

int counter = 0;

struct strings
{
    char** strs;
    int    cap;
    int    size; 
};

void add(struct strings* strs, char* str) {
    if (strs->cap == strs->size){
        strs->cap *= 2;

        strs->strs = (char**)realloc(strs->strs, sizeof(char*) * strs->cap);
        if (strs->strs == NULL){
            fprintf(stderr, "Unable to allocate memory realloc\n");
            exit(FAILD_ALLOCATE_MEMORY); 
        }
    }
    
    strs->strs[strs->size++] = strdup(str);
    if (strs->strs[strs->size - 1] == NULL){
        fprintf(stderr, "Unable to allocate memory strdup\n");
        exit(FAILD_ALLOCATE_MEMORY); 
    }
}

void initialize_strings(struct strings* strs) {
    strs->cap  = STR_CAP;
    strs->size = 0;
    strs->strs = (char**)malloc(sizeof(char*) * STR_CAP);

    if (strs->strs == NULL){
        fprintf(stderr, "Unable to allocate memory malloc\n");
        exit(FAILD_ALLOCATE_MEMORY); 
    }
}

void clear(struct strings* strs) {
    for (int i = 0; i < strs->size; i++){
        free(strs->strs[i]);
    }
    free(strs->strs);

    strs->cap  = 0;
    strs->size = 0;
    strs->strs = NULL;
}

char* capacity_up(char* str, int* capacity, int size) {
    if (*capacity == size + 1){
        *capacity *= 2;

        str = (char*) realloc(str, *capacity);
        if (str == NULL){
            fprintf(stderr, "Unable to allocate memory realloc\n");
            exit(FAILD_ALLOCATE_MEMORY); 
        }        
    }
    return str;
}

void reader(struct strings* strs) {
    char c;
    int counter  = 0;
    int capacity = STR_CAP;

    char* str = (char*)malloc(sizeof(char) * STR_CAP);
    if (str == NULL){
        fprintf(stderr, "Unable to allocate memory malloc\n");
        exit(FAILD_ALLOCATE_MEMORY); 
    }

    enum STATES{START, SPACE, DQUOTES, SYMBOL, SPECIAL_SYMBOL} state = START;
    while((c = getchar()) != '\n'){

        switch (state)
        {
        case START:
            if (c == '"'){
                state = DQUOTES;
            } else if (c == '&' || c == '>' || c == '<' || c == '|' || c == '(' || c == ')' || c == ';') {
                state = SPECIAL_SYMBOL;
                ungetc(c, stdin);
            } else if (isspace(c)){
                state = SPACE;
            } else{
                state = SYMBOL;
                ungetc(c, stdin);
            }
            if (counter){
                str[counter] = '\0';
                counter = 0;
                add(strs, str);
            } 
            break;

        case SPACE:
            if (!isspace(c)){
                state = START;
                ungetc(c, stdin);
            }
            break;
        
        case DQUOTES:
            if (c != '"'){
                str = capacity_up(str, &capacity, counter);
                str[counter++] = c; 
            } else if (c != '&' && c != '>' && c != '<' && c != '|' && c != '(' && c != ')' && c != ';' && !isspace(c)){
                state = SYMBOL;
            } else{
                state = START;
            }  
            break;
        
        case SYMBOL:
            if (c == '&' || c == '>' || c == '<' ||  c == '|' || c == '(' || c == ')' || c == ';' || isspace(c)) {
                ungetc(c, stdin);
                state = START;
            } else if (c == '"' ){
                state = DQUOTES;   
            } else{
                str = capacity_up(str, &capacity, counter);
                str[counter++] = c;
            }
            break;
        
        case SPECIAL_SYMBOL:
            str[counter++] = c;
            if (c == '&' || c == '>' || c == '|'){
                c = getchar();
                if (c == '&' || c == '>' || c == '|'){
                    str[counter++] = c;
                } else{ 
                    ungetc(c, stdin);
                }
            } else if(c != '<' && c != '(' && c != ')' && c != ';'){
                ungetc(c, stdin);
            }
            state = START;
            break;
        }
    }
    if(counter){
        str[counter] = '\0';
        add(strs, str);
    }
    free(str);
}

void check_conditions(char** str, int begin, int end) {
    if (begin + 1 == end){
        printf("%s", "bash: syntax error near unexpected token 'newline'\n");
        exit(SYNTAX_ERROR);
    }
    if((!strcmp(str[begin], ">") || !strcmp(str[begin], ">>") || !strcmp(str[begin], "<")) &&
               (!strcmp(str[begin + 1], "(")  || !strcmp(str[begin + 1], ")") || !strcmp(str[begin + 1], "|")
            || !strcmp(str[begin + 1], "||") || !strcmp(str[begin + 1], "&") || !strcmp(str[begin + 1], "&&")
            || !strcmp(str[begin + 1], ";")  || !strcmp(str[begin + 1], ">") || !strcmp(str[begin + 1], ">>")
            || !strcmp(str[begin + 1], "<"))) {
        printf("bash: syntax error near unexpected token '%s'\n", str[begin + 1]);
        exit(SYNTAX_ERROR);
    }
}

int simple_command(char** str, int begin, int end) {
    int pid;
    char* word = NULL;
    int status = 0;

    if(str[end]) {
        word = str[end];
        str[end] = NULL;
    }

    if(!strcmp(str[0], "cd")){
        if (chdir(str[1]) != 0){
            fprintf(stdin, "cd error : too many arguments\n");
        }
    }else{
        if ((pid = fork()) == 0){
            execvp(str[begin], str + begin); 
            fprintf(stderr, "%s: command not found\n", str[begin]);
            exit(COMMAND_NOT_FOUND);
        } else{
            wait(&status);
        }
    }

    if(word) 
        str[end] = word;

    return WEXITSTATUS(status);
}

void errors_in_open_close_dup(int err, int op) {
    if (err < 0){
        switch (op)
        {
        case 1:
            fprintf(stderr, "%d open error with code: \n", errno);
            exit(OPEN_ERROR);
            break;
        case 2:
            fprintf(stderr, "%d close error with code: \n", errno);
            exit(CLOSE_ERROR);
            break;
        case 3:
            fprintf(stderr, "%d dup error with code: \n", errno);
            exit(DUP_ERROR);
            break;
        default:
            exit(UNEXPECTED_ERROR);
            break;
        }
    }
}

int redirection_inp_out(char** str, int begin, int end) {
    int file;
    int result    = 0;
    int error     = 0;
    int new_end   = begin;
    int cmd_begin = begin; 
    int cmd_end   = end;
    char* word    = NULL;

    int stdin_backup  = dup(0);
    int stdout_backup = dup(1);
    errors_in_open_close_dup(stdin_backup,  3);
    errors_in_open_close_dup(stdout_backup, 3);

    while(new_end < end) {
        word = str[new_end++];
        
        if(!strcmp(word, ">") || !strcmp(word, ">>") || !strcmp(word, "<")){
            check_conditions(str, new_end - 1, end);

            if(!strcmp(word, ">")) {
                file = open(str[new_end], O_CREAT | O_WRONLY | O_TRUNC, 0666);
                errors_in_open_close_dup(file, 1);
                error = dup2(file, 1);
                errors_in_open_close_dup(error, 3);

            } else if(!strcmp(word, ">>")){
                file = open(str[new_end], O_APPEND | O_WRONLY | O_TRUNC, 0666);
                errors_in_open_close_dup(file, 1);
                error = dup2(file, 1);
                errors_in_open_close_dup(error, 3);

            } else {
                file = open(str[new_end], O_RDONLY);
                errors_in_open_close_dup(file, 1);
                error = dup2(file, 0);
                errors_in_open_close_dup(error, 3);
            }
            
            close(file);
            errors_in_open_close_dup(error, 2);
            cmd_begin = ++new_end;

            if(cmd_end == end)
                cmd_end = new_end - 1;
        } else{
            cmd_end = new_end;
        } 
    }

    if(cmd_begin == end) {
        result = simple_command(str, begin, cmd_end);
    } else {
        result = simple_command(str, cmd_begin, end);
    }

    error = dup2(stdout_backup, 0);
    errors_in_open_close_dup(error, 3);
    error = dup2(stdin_backup, 1);
    errors_in_open_close_dup(error, 3);

    return result;
}

void background_process_check() {
    int status;
    
    while(counter > 0) {
        int pid = wait(&status);
        if (WIFEXITED(status)){
			printf("[%d] exited; status = %d\n", pid, WEXITSTATUS (status));
		} else if (WIFSIGNALED (status)){
			printf("[%d] killed by signal %d\n", pid, WTERMSIG (status)); 
		} else if (WIFSTOPPED(status)){
			printf("[%d] stopped by signal %d\n", pid, WSTOPSIG(status));
	    } else if (WIFCONTINUED(status)){
			printf("[%d] continued\n", pid);
		}
    }
    counter--;
}

int command(char** str, int begin, int end) {
    int fd[2];
    int conv      = 0;
    int new_begin = begin;
    int new_end   = begin;
    char* word    = NULL;
    int saved_stdin = dup(STDIN_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);

    int pid_counter = 0;
    while(new_end < end){
        word = str[new_end++];

        if(!strcmp(word, "|") || (new_end == end && conv > 0)){
            conv = 1;
            pid_counter++;
            pipe(fd);
            if(fork() == 0) {
                if (new_end != end) dup2(fd[1], 1);
                close(fd[0]); close(fd[1]);
                if(!strcmp(word, "|")) str[new_end - 1] = NULL;
                
                execvp(str[new_begin], str + new_begin); 
                fprintf(stderr, "%s: command not found\n", str[new_begin]);
                exit(COMMAND_NOT_FOUND);
            }
            new_begin = new_end;
            dup2(fd[0], 0);
            close(fd[0]); close(fd[1]);
        }
    }
    int status = 0;
    for(int i = 0; i < pid_counter; i++) 
        wait(&status);
        
    if(conv == 0) {
        int r = 0;
        r = redirection_inp_out(str, new_begin, end);
        return r;
    }
    dup2(saved_stdin, STDIN_FILENO);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdin); close(saved_stdout);

    return WEXITSTATUS(status);
}

void parse_command(char** str, int begin, int end) {
    char* word = NULL;
    int new_begin = begin;
    int new_end = begin;

    while(new_end < end) {
        word = str[new_end++];

        if(!strcmp(word, "&&")) {
            if (command(str, new_begin, new_end - 1) != 0) return;
            new_begin = new_end;
        }
        if(!strcmp(word, "||")) {
            if (command(str, new_begin, new_end - 1) == 0) return;
            new_begin = new_end;
        }
    }

    if(strcmp(word, "&&") && strcmp(word, "||")) 
        command(str, new_begin, end);
}

void parse_shell_command(char** str, int size) {
    int begin = 0;
    int end = 0;
    char* word = NULL;
    
    while(end < size){  
        word = str[end++];
        
        if (!strcmp(word,";")){
            parse_command(str, begin, end - 1);
            begin = end;
        }
        if (!strcmp(word, "&")){
            if (fork() == 0){
                parse_command(str, begin, end - 1);
                exit(0);
            }
            begin = end;
        }
    }
    
    if (strcmp(word,";") && strcmp(word,"&"))
        parse_command(str, begin, size);
}

void executor(struct strings* strs) {
    parse_shell_command(strs->strs, strs->size);

    char cwd[PATH_MAX];
    getcwd(cwd, PATH_MAX);
    printf("%s$ ", cwd);
}

int main(int argc, char** argv) {
    struct strings words;
    initialize_strings(&words);

    char cwd[PATH_MAX];
    getcwd(cwd, PATH_MAX);
    printf("%s$ ", cwd);

    while(1) {
        reader(&words);
        executor(&words);
        clear(&words);
        initialize_strings(&words);
    }

    return 0;
}
