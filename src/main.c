
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

extern int g_unclosed_quote;

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
    char *curr_line = NULL;
    size_t line_buf_size = 0;

    char *accum_input = NULL;
    size_t accum_len = 0;
    int first_line = 1;

    while (1) {
        // Показываем приглашение
        if (!first_line) {
            printf("> ");
            fflush(stdout);
        }

        ssize_t n = getline(&curr_line, &line_buf_size, stdin);
        if (n < 0) {
            free(curr_line);
            free(accum_input);
            return NULL; // EOF
        }

        // убрать \n
        if (n > 0 && curr_line[n-1] == '\n') curr_line[n-1] = '\0';

        // Проверка на backslash continuation
        size_t L = strlen(curr_line);
        int cont = (L > 0 && curr_line[L-1] == '\\');
        if (cont) curr_line[L-1] = '\0';

        size_t add = strlen(curr_line);
        
        // вычисляем нужный размер
        size_t need_newline = (accum_len > 0 && !first_line) ? 1 : 0;
        char *new_accum = realloc(accum_input, accum_len + add + need_newline + 1);
        if (!new_accum) {
            free(curr_line);
            free(accum_input);
            return NULL;
        }
        accum_input = new_accum;

        //\n перед копированием новой строки
        if (need_newline) {
            accum_input[accum_len] = '\n';
            accum_len++;
        }

        memcpy(accum_input + accum_len, curr_line, add);
        accum_len += add;
        accum_input[accum_len] = '\0';

        // Если был backslash - продолжаем ввод
        if (cont) {
            first_line = 0;
            continue;
        }

                
        // Пробуем токенизировать
        Token *test_tokens = tokenize(accum_input);
                
        if (!test_tokens) {
            if (g_unclosed_quote) {
                // Незакрытые кавычки - продолжаем ввод
                first_line = 0;
                continue;
            } else {
                // Другая ошибка
                free(curr_line);
                free(accum_input);
                return NULL;
            }
        }

        // Токены получены успешно
        free_tokens(test_tokens);
   
        break;
    }

    free(curr_line);
    return accum_input;
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
            print_ast(ast);
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