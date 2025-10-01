#pragma once
#include "token.h"
#include <stdlib.h>

extern const char word_delimeters[];
typedef enum{
    SIMPLE_WORD,
    WORD_IN_QUOTES,
    DELIMETERS,
    END_LINE,
} SimpleWord;

int is_space(char c);
int is_delimiter(char c);
int define_simple_word(char c);
int define_word_len(const char* word, int offset);
char* create_word(int word_len); //Не могу явно в case иннициализировать массив (((
Token *create_token(TokenType type, char *value);
void print(Token **array);
Token **tokenize(const char *input);
