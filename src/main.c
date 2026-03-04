#define _POSIX_C_SOURCE 200809L 

#include "../inc/lexer.h"      
#include "../inc/ast.h"     
#include "../inc/parser.h"  
#include "../inc/execution.h"
#include "../inc/jobs.h"       
#include "../inc/builtin.h"    
#include "../inc/history.h"  
#include <stdio.h>            
#include <string.h>           
#include <stdlib.h>         
#include <unistd.h>             
#include <limits.h>             
#include <sys/wait.h>  
#include <signal.h>             
#include <termios.h>            
#include <sys/ioctl.h>          

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

int enable_raw_mode(struct termios *orig) { 
    if (tcgetattr(STDIN_FILENO, orig) == -1) { 
        return -1;                   
    }
    //копируем настройки для raw 
    struct termios raw = *orig;      
    raw.c_lflag &= ~(ICANON | ECHO); 
    raw.c_cc[VMIN] = 1;              
    raw.c_cc[VTIME] = 0;             

    return tcsetattr(STDIN_FILENO, TCSANOW, &raw); 
}

void disable_raw_mode(const struct termios *orig) { 
    if (orig) {                        
        tcsetattr(STDIN_FILENO, TCSANOW, orig); 
    }
}

void redraw_line(int is_continuation, const char *buffer) { 
    printf("\r");                     
    if (is_continuation) {              
        printf("> ");                 
    } else {
        print_prompt();                 
    }

    if (buffer) {                       
        fputs(buffer, stdout);          
    }

    printf("\x1b[K");                 
    fflush(stdout);                     
}

static char *read_single_line(int is_continuation) { 
    struct termios orig = {0};          
    if (enable_raw_mode(&orig) != 0) {  
        return NULL;                    
    }

    // выделяем место под буффер ввода
    size_t cap = 128;                   
    size_t len = 0;                     
    char *buffer = malloc(cap);         
    if (!buffer) {                      
        disable_raw_mode(&orig);        
        return NULL;                    
    }
    buffer[0] = '\0';                  

    char *saved_current = NULL;         
    int history_pos = -1;               

    redraw_line(is_continuation, buffer); 

    while (1) {                         
        unsigned char c = 0;            
        ssize_t r = read(STDIN_FILENO, &c, 1); 
        if (r <= 0) {                   
            free(saved_current);        
            free(buffer);               
            disable_raw_mode(&orig);    
            return NULL;                
        }

        if (c == '\n' || c == '\r') {  // enter 
            putchar('\n');             
            buffer[len] = '\0';        
            free(saved_current);        
            disable_raw_mode(&orig);    
            return buffer;              
        }

        if (c == 4) {              // ctrl+d     
            if (len == 0) {             
                free(saved_current);
                free(buffer);
                disable_raw_mode(&orig);
                return NULL;            
            }
            continue;                   
        }

        if (c == 127 || c == 8) {   // backspace/del     
            if (len > 0) {              
                len--;                   
                buffer[len] = '\0';    
                redraw_line(is_continuation, buffer); 
            }
            continue;                   
        }

        if (c == 27) {  // esc  
            
            //есть ли два байта следующими?
            int pending = 0;            
            ioctl(STDIN_FILENO, FIONREAD, &pending); 
            if (pending < 2) {          
                continue;
            }

            // пропускаем два байта
            unsigned char seq[2];       
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) continue; 
            if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue; 


            if (seq[0] == '[') {        
                if (seq[1] == 'A') {    // стрелка вверх
                    if (history_pos + 1 < history_size()) { //была ли запись?
                        if (history_pos == -1) { 
                            // первый заход в историю
                            free(saved_current);  
                            saved_current = strdup(buffer); 
                            if (!saved_current) saved_current = NULL; 
                        }


                        // копируем новую запись и переписываем
                        history_pos++;           
                        const char *h = history_get_from_tail(history_pos); 
                        if (h) {                  
                            size_t hl = strlen(h); 
                            if (hl + 1 > cap) {   
                                size_t new_cap = hl + 64; 
                                char *tmp = realloc(buffer, new_cap); 
                                if (!tmp) {       
                                    free(saved_current);
                                    free(buffer);
                                    disable_raw_mode(&orig);
                                    return NULL;
                                }
                                buffer = tmp;     
                                cap = new_cap;    
                            }

                            memcpy(buffer, h, hl + 1); 
                            len = hl;                  
                            redraw_line(is_continuation, buffer); 
                        }
                    }
                } else if (seq[1] == 'B') { //стрелка вниз
                    if (history_pos >= 0) { // проверяем возможность спуска вниз
                        history_pos--;      
                        const char *h;

                        // при спуске если есть запись пишем ее/возвращаем что было
                        if(history_pos >= 0){
                            h = history_get_from_tail(history_pos);
                        } else {
                            if(saved_current) { 
                                h = saved_current;
                            } else { 
                                h = "";
                            }
                        }
                        
                        if (!h) h = "";    

                        // переписываем запись
                        size_t hl = strlen(h); 
                        if (hl + 1 > cap) {   
                            size_t new_cap = hl + 64; 
                            char *tmp = realloc(buffer, new_cap); 
                            if (!tmp) {       
                                free(saved_current);
                                free(buffer);
                                disable_raw_mode(&orig);
                                return NULL;
                            }
                            buffer = tmp;     
                            cap = new_cap;    
                        }

                        memcpy(buffer, h, hl + 1); 
                        len = hl;                  

                        if (history_pos < 0) {     
                            free(saved_current);    
                            saved_current = NULL;
                        }

                        redraw_line(is_continuation, buffer); 
                    }
                }
            }
            continue;                   
        }

        // проверка на случай если в конце переполнился буфер
        if (len + 2 > cap) {            
            size_t new_cap = cap * 2;   
            char *tmp = realloc(buffer, new_cap); 
            if (!tmp) {                 
                free(saved_current);
                free(buffer);
                disable_raw_mode(&orig);
                return NULL;
            }
            buffer = tmp;               
            cap = new_cap;              
        }

        buffer[len++] = c;        
        buffer[len] = '\0';            
        putchar(c);                     
        fflush(stdout);                 
    }
}

char *read_command_line(void) {         
    char *accum_input = NULL;           
    size_t accum_len = 0;               
    int first_line = 1;              //первая ли строка?   
    int join_without_newline = 0;    // склеивать без \n?    

    while (1) {                         
        char *curr_line = read_single_line(first_line ? 0 : 1); 

        if (!curr_line) {               
            free(accum_input);          
            return NULL;                
        }

        // есть ли конкатенация?
        size_t L = strlen(curr_line);   
        int is_continuation = 0;        
        if (L > 0 && curr_line[L-1] == '\\'){ 
            curr_line[L-1] = '\0';     
            is_continuation = 1;        
        }

        size_t add = strlen(curr_line); 
        
        int need_newline;
        if (accum_len > 0 && !first_line && !join_without_newline) {
            need_newline = 1;
        } else {
            need_newline = 0;
        }

        // расширяем накопленную строку
        char *new_accum = realloc(accum_input, accum_len + add + need_newline + 1); 
        if (!new_accum) {               
            free(curr_line);
            free(accum_input);
            return NULL;
        }
        accum_input = new_accum;         

        if (need_newline) {             
            accum_input[accum_len] = '\n'; 
            accum_len++;                
        }

        // копируем в накопленную строоку
        memcpy(accum_input + accum_len, curr_line, add); 
        accum_len += add;               
        accum_input[accum_len] = '\0'; 

        // идем на новую строку, если ввод не закончился
        if (is_continuation) {          
            first_line = 0;             
            join_without_newline = 1;   
            continue;                   
        }

        join_without_newline = 0;       

        Token *test_tokens = tokenize(accum_input); 
             
        // обработка незакрытой кавычки
        if (!test_tokens) {             
            if (g_unclosed_quote) {     
                first_line = 0;
                free(curr_line);
                continue;
            } else {
                free(curr_line);        
                free(accum_input);
                return NULL;
            }
        }

        // обработка недописанных команд
        int needs_continuation = 0;     
        int last_meaningful = -1;       
        for (int i = 0; test_tokens[i].type != TOKEN_EOF; i++) { 
            last_meaningful = i;
        }
        if (last_meaningful >= 0) {     
            TokenType lt = test_tokens[last_meaningful].type; 
            if (lt == TOKEN_PIPE || lt == TOKEN_PIPE_AMPER || 
                lt == TOKEN_OR  || lt == TOKEN_AND) {
                needs_continuation = 1; 
            }
        }

        free_tokens(test_tokens);       

        if (needs_continuation) {       
            first_line = 0;             
            join_without_newline = 0;   
            free(curr_line);            
            continue;                   
        }
   
        free(curr_line);                
        break;                          
    }
    return accum_input;                 
}


int main() {                            

    history_init();                     
    init_shell();                       
    int exit_code = 0;                  

    while(1){                           
        check_background_jobs();        

        char *cmd = read_command_line(); 

        if(!cmd) {                      
            printf("\n");            
            break;                      
        }

        if (cmd[0] == '\0') {          
            free(cmd);                  
            continue;                   
        }

        history_add(cmd);               

        Token *tokens = tokenize(cmd);  
        free(cmd);                      
 
        if (!tokens) continue;          

        ASTNode *ast = parse(tokens);   
        if (ast) {                      
            execute(ast);               

            free_ast(ast);              
        }
        
        free_tokens(tokens);            

        if (shell_exit_requested()) {   
            exit_code = shell_exit_code(); 
            break;                      
        }

    }

    history_shutdown();                 
    return exit_code;                   
}