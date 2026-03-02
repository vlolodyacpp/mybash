#include "../inc/parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

const char *token_lexeme_or_eof(Token *tok) {
    if (!tok || tok->type == TOKEN_EOF) {
        return "EOF";
    }
    if (tok->value && tok->value[0] != '\0') {
        return tok->value;
    }
    return "token";
}


ASTNode *parse(Token *tokens){
    if(!tokens || tokens[0].type == TOKEN_EOF){
        return NULL;
    }

    Token *curr = tokens;
    ASTNode *root = parse_list(&curr);

    if (!root) {
        fprintf(stderr, "Syntax error near '%s'\n", token_lexeme_or_eof(curr));
        return NULL;
    }

    if (curr->type != TOKEN_EOF && curr->type != TOKEN_RPAREN) {
        fprintf(stderr, "Syntax error: unexpected token '%s'\n", token_lexeme_or_eof(curr));
        free_ast(root);
        return NULL;
    }

    return root;
}


ASTNode *parse_list(Token **curr){
    ASTNode *left = parse_logical(curr);
    if(!left) return NULL;

    while(1){
        if (match(curr, TOKEN_AMPER)){ // фоновое выполнение
            left = create_unary(NODE_BACKGROUND, left);
            
            // если достигли конца, возвращаем результат
            if((*curr) -> type == TOKEN_EOF) return left;
            
            // опционально игнорируем ';' после '&'
            if((*curr) -> type == TOKEN_SEMICOL) (*curr)++;

            // если есть еще команды, парсим их и соединяем через ;
            if ((*curr)->type != TOKEN_EOF && (*curr)->type != TOKEN_RPAREN) {
                ASTNode *right = parse_list(curr);
                if (right){
                    left = create_binary(NODE_SEQUENCE, left, right);
                }
            }

            return left;
            
        } else if(match(curr, TOKEN_SEMICOL)){ // ; 
            // Если ';' в конце или перед ')', просто возвращаем левую часть
            if ((*curr) -> type == TOKEN_EOF  || (*curr)->type == TOKEN_RPAREN){
                return left;
            }

            // Парсим правую часть после ';'
            ASTNode *right = parse_list(curr);

            // Соединяем обе части через узел ;
            if (right){
                left = create_binary(NODE_SEQUENCE, left, right);
            }
            return left;

        } else {
            // не встретили ни ';', ни '&' - возвращаем текущий узел
            break;
        }
    }
    return left;

}

ASTNode *parse_logical(Token **curr){
    ASTNode *left = parse_pipeline(curr);

    if(!left) return NULL;

    while ((*curr) -> type == TOKEN_AND || (*curr) -> type == TOKEN_OR){
        TokenType op = (*curr) -> type;
        (*curr)++; // Пропускаем оператор && или ||

        // Парсим правую часть выражения
        ASTNode *right = parse_pipeline(curr);
        if(!right) { 
            fprintf(stderr, "Syntax error: expected command after '%s'\n", op == TOKEN_AND ? "&&" : "||");
            free_ast(left);
            return NULL;
        }
        
        // Создаем узел AND или OR
        NodeType node_type = (op == TOKEN_AND) ? NODE_AND : NODE_OR;
        left = create_binary(node_type, left, right);
    }

    return left;

}

ASTNode *parse_pipeline(Token **curr){
    ASTNode *left = parse_factor(curr);
    if (!left) return NULL;

    // цикл для обработки цепочки конвейеров
    while ((*curr) -> type == TOKEN_PIPE || (*curr) -> type == TOKEN_PIPE_AMPER) { 
        TokenType op = (*curr) -> type;
        (*curr)++; // Пропускаем оператор | или |&

        ASTNode *right = parse_factor(curr);
        if(!right){
            fprintf(stderr, "Syntax error: expected command after pipe\n");
            free_ast(left);
            return NULL;
        } 

        // Создаем узел PIPE или PIPE_STDERR
        NodeType node_type = (op == TOKEN_PIPE) ? NODE_PIPE : NODE_PIPE_STDERR;
        left = create_binary(node_type, left, right);
    }

    return left;

}

ASTNode *parse_factor(Token **curr){
    if((*curr) -> type == TOKEN_LPAREN) { 
        (*curr)++; // Пропускаем '('

        ASTNode *inner = parse_list(curr);
        if (!inner) {
            fprintf(stderr, "error inside ()\n");
            return NULL;
        }
       
        if ((*curr)->type != TOKEN_RPAREN) {
            fprintf(stderr, "Syntax error: expected ')'\n");
            free_ast(inner);
            return NULL;
        }
        (*curr)++; 

        return create_unary(NODE_SUB, inner);
    }

    return parse_simple_command(curr);
}


ASTNode *parse_simple_command(Token **curr) {
    if ((*curr) -> type != TOKEN_WORD && (*curr) -> type != TOKEN_WORD_IN_QUOTES && 
        (*curr) -> type != TOKEN_REDIR_IN && (*curr) -> type != TOKEN_REDIR_OUT && 
        (*curr) -> type != TOKEN_REDIR_APPEND && (*curr) -> type != TOKEN_AMPER_REDIR_IN && 
        (*curr) -> type != TOKEN_AMPER_REDIR_APPEND) {
        return NULL;
    }

    int capacity = 8;
    int argc = 0;
    char **argv = malloc(capacity * sizeof(char*));
    int *arg_no_expand = calloc(capacity, sizeof(int));
    Redirection *redir_head = NULL;

    if (!argv || !arg_no_expand) {
        free(argv);
        free(arg_no_expand);
        return NULL;
    }


    while (1) {
        // обрабатываем слова (имя команды и аргументы)
        if ((*curr) -> type == TOKEN_WORD || (*curr) -> type == TOKEN_WORD_IN_QUOTES) {
            if (argc >= capacity - 1) {
                int new_capacity = capacity * 2;
                char **new_argv = malloc(new_capacity * sizeof(char*));
                int *new_arg_no_expand = calloc(new_capacity, sizeof(int));
                if (!new_argv || !new_arg_no_expand) {
                    free(new_argv);
                    free(new_arg_no_expand);
                    for (int i = 0; i < argc; ++i) free(argv[i]);
                    free(argv);
                    free(arg_no_expand);
                    return NULL;
                }

                for (int i = 0; i < argc; ++i) {
                    new_argv[i] = argv[i];
                    new_arg_no_expand[i] = arg_no_expand[i];
                }

                free(argv);
                free(arg_no_expand);
                argv = new_argv;
                arg_no_expand = new_arg_no_expand;
                capacity = new_capacity;
            }
           
            argv[argc++] = strdup((*curr) -> value);
            arg_no_expand[argc - 1] = ((*curr)->type == TOKEN_WORD_IN_QUOTES) ? 1 : 0;
            (*curr)++;
        } // обрабатываем перенаправления 
        else if ((*curr) -> type == TOKEN_REDIR_IN || (*curr) -> type == TOKEN_REDIR_OUT || 
                (*curr) -> type == TOKEN_REDIR_APPEND || (*curr) -> type == TOKEN_AMPER_REDIR_IN || 
                (*curr) -> type == TOKEN_AMPER_REDIR_APPEND) {
            
            // определяем тип перенаправления
            RedirType r_type;
            switch ((*curr) -> type) {
                case TOKEN_REDIR_IN: r_type = REDIR_IN; break;           // <
                case TOKEN_REDIR_OUT: r_type = REDIR_OUT; break;         // >
                case TOKEN_REDIR_APPEND: r_type = REDIR_APPEND; break;   // >>
                case TOKEN_AMPER_REDIR_IN: r_type = REDIR_ERR_OUT; break; // &>
                case TOKEN_AMPER_REDIR_APPEND: r_type = REDIR_ERR_APPEND; break; // &>>
                default: r_type = REDIR_OUT; break;
            }
            (*curr)++; // пропускаем символ перенаправления (<, >, >>, &>, &>>)

            // проверяем, что после перенаправления идет имя файла
            if ((*curr) -> type == TOKEN_WORD || (*curr) -> type == TOKEN_WORD_IN_QUOTES) {
                // добавляем перенаправление в связный список
                add_redir(&redir_head, r_type, (*curr) -> value);
                (*curr)++; // пропускаем имя файла
            } else {
                fprintf(stderr, "Syntax error: expected filename\n");
                for (int i = 0; i < argc; ++i) free(argv[i]);
                free(argv);
                free(arg_no_expand);
                return NULL;
            }
        } else {
            break;
        }
    }

    argv[argc] = NULL;
    
    return create_command(argv, arg_no_expand, redir_head, argc);
}


int match(Token **curr, TokenType type){
    if((*curr) -> type == type){
        (*curr)++;  // Потребляем токен
        return 1;
    }
    return 0;
}
