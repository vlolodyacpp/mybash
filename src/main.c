
#define _POSIX_C_SOURCE 200809L

#include "../inc/lexer.h"
#include "../inc/ast.h"
#include "../inc/parser.h" 
#include "../inc/execution.h" 
#include "../inc/jobs.h" 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#include <signal.h>


void print_prompt(){ 
    char hostname[HOST_NAME_MAX];
    char cwd[PATH_MAX];
    char *username = getenv("USER");

    gethostname(hostname, HOST_NAME_MAX);
    getcwd(cwd, PATH_MAX);

    printf("\033[1;35m%s@%s\033[0m:\033[1;34m%s\033[0m$ ", username ? username : "user", hostname, cwd);
    fflush(stdout);

}



void test_parser(const char *input) {
    printf("\n>>> Parsing: \"%s\"\n", input);
    
    Token *tokens = tokenize(input);
    if (!tokens) return;
    
    ASTNode *ast = parse(tokens);
    
    if (ast) {
        print_ast(ast);
        execute(ast);
        free_ast(ast);
    } else {
        printf("Parser returned NULL (empty or error)\n");
    }

    free_tokens(tokens);
}

static char *read_command_line(void) {
    char *line = NULL;
    size_t cap = 0;

    char *acc = NULL;
    size_t acc_len = 0;

    while (1) {
        ssize_t n = getline(&line, &cap, stdin);
        if (n < 0) {
            free(line);
            free(acc);
            return NULL; // EOF
        }

        // убрать \n
        if (n > 0 && line[n-1] == '\n') line[n-1] = '\0';

        size_t L = strlen(line);
        int cont = (L > 0 && line[L-1] == '\\');
        if (cont) line[L-1] = '\0';

        size_t add = strlen(line);
        char *nacc = realloc(acc, acc_len + add + 2);
        if (!nacc) {
            free(line);
            free(acc);
            return NULL;
        }
        acc = nacc;

        memcpy(acc + acc_len, line, add);
        acc_len += add;
        acc[acc_len] = '\0';

        if (!cont) break;

        // чтобы слова не слипались
        acc[acc_len++] = ' ';
        acc[acc_len] = '\0';

        printf("> ");
        fflush(stdout);
    }

    free(line);
    return acc;
}


int main() {

    init_shell();

    while(1){
        check_background_jobs();
        print_prompt();

        char *cmd = read_command_line();

        if(!cmd) { 
            printf("\n");
            break;
        }

        if (cmd[0] == '\0') {
            free(cmd);
            continue;
        }

        Token *tokens = tokenize(cmd);
        free(cmd);
        if (!tokens) continue;

        ASTNode *ast = parse(tokens);
        if (ast) {
            execute(ast);

            free_ast(ast);
        }
        
        free_tokens(tokens);


    }

    
    // test_parser("ls -la");
    
    // test_parser("help");
    // test_parser("ls | grep .c");
    // test_parser("cat file | grep search | sort");

    // test_parser("echo hello > out.txt");
    // test_parser("cat < in.txt >> append.txt");
    
    // test_parser("ls > out.txt -la"); 
    
    // test_parser("make && ./main || echo fail");

    // test_parser("sleep 10 &");
    // test_parser("cmd1 ; cmd2");
    // test_parser("cmd1 & cmd2"); 
  
    // test_parser("gcc main.c -o main && ./main | grep output > result.txt || echo error");

    
    return 0;
}