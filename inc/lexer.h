#pragma once
#include "token.h"

const char word_delimeters[] = " \t;|&<>"; 
typedef enum{
    SIMPLE_WORD,
    QUOTE,
    DELIMETERS,
    END_LINE,
} SimpleWord;

int is_space(char c);
int is_delimiter(char c);
int define_simple_word(char c);
int define_word_len(char* word, int offset);
char* create_word(int word_len); //Не могу явно в case иннициализировать массив (((
Token *create_token(TokenType type, const char *value);
Token *tokenize(const char *input);
