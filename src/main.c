#include "../inc/lexer.h"
#include "../inc/ast.h"
#include "../inc/parser.h" 
#include "../inc/execution.h" 
#include <stdio.h>
#include <string.h>

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
    printf("=== MyBash Full Pipeline Test ===\n");
    
    test_parser("ls -la");
    

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