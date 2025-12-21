
#define _POSIX_C_SOURCE 200809L

#include "../inc/lexer.h"
#include "../inc/ast.h"
#include "../inc/parser.h" 
#include "../inc/execution.h" 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>




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

int main() {
    char buffer[1024];

    while(1){
        print_prompt();

        if (!fgets(buffer, sizeof(buffer), stdin)){
            printf("\n");
            break;
        }

        buffer[strcspn(buffer, "\n")] = 0;
        if (strlen(buffer) == 0) continue;

        Token *tokens = tokenize(buffer);
        if (!tokens) continue;

        ASTNode *ast = parse(tokens);
        if (ast) {
            execute(ast);
            free_ast(ast);
        }
        
        free_tokens(tokens);


    }
    // printf("=== MyBash Full Pipeline Test ===\n");
    
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