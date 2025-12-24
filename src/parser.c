#include "../inc/parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


ASTNode *parse(Token *tokens){
    if(!tokens || tokens[0].type == TOKEN_EOF){
        return NULL;
    }

    Token *curr = tokens;
    return parse_list(&curr);
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

    // Цикл для обработки цепочки логических операторов
    while ((*curr) -> type == TOKEN_AND || (*curr) -> type == TOKEN_OR){
        TokenType op = (*curr) -> type;
        (*curr)++; // Пропускаем оператор && или ||

        // Парсим правую часть выражения
        ASTNode *right = parse_pipeline(curr);
        if(!right) { 
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
            fprintf(stderr, "Syntax error: unknown command");
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
    Redirection *redir_head = NULL;


    while (1) {
        // обрабатываем слова (имя команды и аргументы)
        if ((*curr) -> type == TOKEN_WORD || (*curr) -> type == TOKEN_WORD_IN_QUOTES) {
            if (argc >= capacity - 1) {
                capacity *= 2;
                argv = realloc(argv, capacity * sizeof(char*));
            }
           
            argv[argc++] = strdup((*curr) -> value);
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
                return NULL;
            }
        } else {
            break;
        }
    }

    argv[argc] = NULL;
    
    return create_command(argv, redir_head, argc);
}


int match(Token **curr, TokenType type){
    if((*curr) -> type == type){
        (*curr)++;  // Потребляем токен
        return 1;
    }
    return 0;
}
