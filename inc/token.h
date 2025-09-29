#pragma once

typedef enum {
    TOKEN_WORD,
    TOKEN_PIPE,
    TOKEN_REDIR_IN,
    TOKEN_REDIR_APPEND,
    TOKEN_AMPER,
    TOKEN_SEMICOL,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_EOF
} TokenType;

typedef struct Token {
    TokenType type;
    char *value;
} Token;