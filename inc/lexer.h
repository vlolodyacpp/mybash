#pragma once
#include "token.h"
#include <stdlib.h>

extern const char word_delimeters[];
typedef enum{
    SIMPLE_WORD,
    WORD_IN_QUOTES,
    DELIMETERS,
} SimpleWord;

int is_space(char c);
int is_delimiter(char c);
int define_simple_word(char c);
int define_word_len(const char* word, int offset);
void print(Token **array);
void free_tokens(Token **array_token);
Token *create_token(TokenType type, char *value);
Token **tokenize(const char *input);
