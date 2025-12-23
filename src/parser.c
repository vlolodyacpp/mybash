#include "../inc/parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


// Начинаем веселье со спуска вниз
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
        if (match(curr, TOKEN_AMPER)){ // Обрабатываем случай &

            left = create_unary(NODE_BACKGROUND, left);
            if((*curr) -> type == TOKEN_EOF) return left;
            if((*curr) -> type == TOKEN_SEMICOL) (*curr)++; // Игнорим ; после &

            // Если все хорошо, соединяем через ;

            if ((*curr)->type != TOKEN_EOF && (*curr)->type != TOKEN_RPAREN) {
                ASTNode *right = parse_list(curr);
                if (right){
                    left = create_binary(NODE_SEQUENCE, left, right);
                }
            }

            return left;
        } else if(match(curr, TOKEN_SEMICOL)){ // Обрабатываем случай ; 

            if ((*curr) -> type == TOKEN_EOF  || (*curr)->type == TOKEN_RPAREN){
                return left;
            }

            ASTNode *right = parse_list(curr);

            if (right){
                left = create_binary(NODE_SEQUENCE, left, right);
            }
            return left;

        } else {
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
        (*curr)++; // Идем дальше для дальнейшей обработки

        ASTNode *right = parse_pipeline(curr);
        if(!right) { 
            free_ast(left);
            return NULL;
        }
        NodeType node_type = (op == TOKEN_AND) ? NODE_AND : NODE_OR;
        left = create_binary(node_type, left, right);
    }

    return left;

}

ASTNode *parse_pipeline(Token **curr){
    ASTNode *left = parse_factor(curr);
    if (!left) return NULL;

    while ((*curr) -> type == TOKEN_PIPE || (*curr) -> type == TOKEN_PIPE_AMPER) { 
        TokenType op = (*curr) -> type;
        (*curr)++;

        ASTNode *right = parse_factor(curr);
        if(!right){
            fprintf(stderr, "Syntax error: unknown command");
            free_ast(left);
            return NULL;
        } 

        NodeType node_type = (op == TOKEN_PIPE) ? NODE_PIPE : NODE_PIPE_STDERR;
        left = create_binary(node_type, left, right);
    }

    return left;

}

ASTNode *parse_factor(Token **curr){

    if((*curr) -> type == TOKEN_LPAREN) { 
        (*curr)++;

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
    // Если не слово и не перенаправление - это не команда
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
        //Cначала обрабатываем аргументы
        if ((*curr) -> type == TOKEN_WORD || (*curr) -> type == TOKEN_WORD_IN_QUOTES) {
            if (argc >= capacity - 1) {
                capacity *= 2;
                argv = realloc(argv, capacity * sizeof(char*));
            }
            argv[argc++] = strdup((*curr) -> value);
            (*curr)++;
        } 
        // Потом обработка перенаправлений
        else if ((*curr) -> type == TOKEN_REDIR_IN || (*curr) -> type == TOKEN_REDIR_OUT || 
                 (*curr) -> type == TOKEN_REDIR_APPEND || (*curr) -> type == TOKEN_AMPER_REDIR_IN || 
                 (*curr) -> type == TOKEN_AMPER_REDIR_APPEND) {
            
            RedirType r_type;
            switch ((*curr) -> type) {
                case TOKEN_REDIR_IN: r_type = REDIR_IN; break;
                case TOKEN_REDIR_OUT: r_type = REDIR_OUT; break;
                case TOKEN_REDIR_APPEND: r_type = REDIR_APPEND; break;
                case TOKEN_AMPER_REDIR_IN: r_type = REDIR_ERR_OUT; break;
                case TOKEN_AMPER_REDIR_APPEND: r_type = REDIR_ERR_APPEND; break;
                default: r_type = REDIR_OUT; break;
            }
            (*curr)++; // Съели символ редиректа

            if ((*curr) -> type == TOKEN_WORD || (*curr) -> type == TOKEN_WORD_IN_QUOTES) {
                add_redir(&redir_head, r_type, (*curr) -> value);
                (*curr)++; // Съели имя файла
            } else {
                fprintf(stderr, "Syntax error: expected filename\n");
                return NULL;
            }
        } 
        else {
            break; // Команда закончилась (встретили | ; & или EOF)
        }
    }

    argv[argc] = NULL;
    return create_command(argv, redir_head, argc);
}



int match(Token **curr, TokenType type){
    if((*curr) -> type == type){
        (*curr)++;
        return 1;
    }
    return 0;
}
