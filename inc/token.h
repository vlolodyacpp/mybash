#pragma once

#define COUNT_DELIMITERS 7
typedef enum {
    TOKEN_WORD, // def word or aguemenet/flag
    TOKEN_PIPE, // |
    TOKEN_REDIR_IN, // >
    TOKEN_REDIR_OUT, // <
    TOKEN_REDIR_APPEND, // >>
    TOKEN_AMPER, // &
    TOKEN_SEMICOL, // ;
    TOKEN_AND, // &&
    TOKEN_OR, // ||
    TOKEN_QUOTE, // "/'
    TOKEN_EOF // ну тут и так понятно
} TokenType;

typedef struct Token {
    TokenType type;
    char *value;

} Token;

