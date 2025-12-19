
#include "../inc/lexer.h"
#include "../inc/token.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define STANDART_CAPACITY 16


const char word_delimeters[] = " \t;|&<>"; 


int is_delimiter(char c){
    for(int i = 0; i < COUNT_DELIMITERS; ++i){
        if(c == word_delimeters[i]) return 1;
    }
    return 0;
}


int is_space(char c){
    return c == ' ' || c == '\t' || c == '\n';
}


int define_simple_word(char c){
    if(c == '"' || c == '\'') return WORD_IN_QUOTES;
    if(is_delimiter(c)) return DELIMETERS;
    return SIMPLE_WORD;
}

int define_word_len(const char* word, int offset){
	int word_len = 0;
	while (1) {
        if (word[offset] == '\0') break;
        if(is_delimiter(word[offset]) || is_space(word[offset])) break;
		word_len++;
		offset++;
	}
	return word_len;
}

void free_tokens(Token *array_token) {
    if (!array_token) return;
    int i;
    for (i = 0; array_token[i].type != TOKEN_EOF; i++) {
        free(array_token[i].value);
    }
    free(array_token[i].value);
    free(array_token);
}


void print(Token *array_token) {
    printf("=== TOKENS ===:\n");
    for (int i = 0; array_token[i].type != TOKEN_EOF; ++i) {
        printf("%d - %s\n", array_token[i].type, array_token[i].value ? array_token[i].value : "(null)");
    }
    printf("=== END ===\n");
}



Token create_token(TokenType type, char *value){
    Token token;
    token.type = type;
    if (value){
       token.value = strdup(value);
    } else {
       token.value = NULL;
    }
    return token;
}



Token *tokenize(const char *input){

    if(!input) { 
        perror("error lol");
        return NULL;
    }

    const size_t len = strlen(input);

    size_t capacity = STANDART_CAPACITY;
    size_t token_cnt = 0;
    Token *array_token = (Token*)malloc(capacity * sizeof(Token));
    if(!array_token){
        perror("malloc error");
        return NULL;
    }

    size_t i = 0;
    while (i < len){
        while(is_space(input[i])) ++i;
        if(i >= len) break;

        if(token_cnt >= capacity){
            capacity *= 2;
            Token *new_array = (Token*)realloc(array_token, capacity * sizeof(Token));
            if(!new_array){
                perror("realloc error");
                free_tokens(array_token);
                return NULL;
            }
            array_token = new_array;
        }

        Token new_token;

        SimpleWord type = define_simple_word(input[i]);
        switch(type){
            case DELIMETERS:
        
                if (input[i] == '&' && input[i+1] == '>' && input[i+2] == '>') {
                    new_token = create_token(TOKEN_AMPER_REDIR_APPEND, "&>>");
                    i += 3;

                } else if (input[i] == '|' && input[i+1] == '&') {
                    new_token = create_token(TOKEN_PIPE_AMPER, "|&");
                    i += 2;
                } else if (input[i] == '|' && input[i+1] == '|') {
                    new_token = create_token(TOKEN_OR, "||");
                    i += 2;
                } else if (input[i] == '&' && input[i+1] == '&') {
                    new_token = create_token(TOKEN_AND, "&&");
                    i += 2;
                } else if (input[i] == '>' && input[i+1] == '>') {
                    new_token = create_token(TOKEN_REDIR_APPEND, ">>");
                    i += 2;
                } else if (input[i] == '&' && input[i+1] == '>') {
                    new_token = create_token(TOKEN_AMPER_REDIR_IN, "&>");
                    i += 2;


                } else if (input[i] == '|') {
                    new_token = create_token(TOKEN_PIPE, "|");
                    i += 1;
                } else if (input[i] == '&') {
                    new_token = create_token(TOKEN_AMPER, "&");
                    i += 1;
                } else if (input[i] == '<') {
                    new_token = create_token(TOKEN_REDIR_IN, "<");
                    i += 1;
                } else if (input[i] == '>') {
                    new_token = create_token(TOKEN_REDIR_OUT, ">");
                    i += 1;
                } else if (input[i] == ';') {
                    new_token = create_token(TOKEN_SEMICOL, ";");
                    i += 1;
                } 

                array_token[token_cnt++] = new_token;
                break;          

            case WORD_IN_QUOTES: {
                char q = input[i];

                int count = 1;
                for( ; input[i + count] != q; ++count); // len_word in "" or ''
                int word_len = count + 1; // count + 1 - размер строки с кавычками, не учитывая послднего символа
                                          

                char *word = (char*)malloc((word_len + 1) * sizeof(char)); // учитываем последний символ, word_len - последняя позиция
                if(!word){
                    perror("malloc error");
                    break;
                }
                strncpy(word, &input[i], word_len);
                word[word_len] = '\0';

                new_token = create_token(TOKEN_WORD_IN_QUOTES, word);
                array_token[token_cnt++] = new_token;
                free(word);
            
                i += word_len;
                break;
            }
            case SIMPLE_WORD: {
                int word_len = define_word_len(input, i);
                char *word = (char*)malloc((word_len + 1) * sizeof(char));
                if(!word){
                    perror("malloc error");
                    break;
                }
                strncpy(word, &input[i], word_len);
                word[word_len] = '\0';

                new_token = create_token(TOKEN_WORD, word);
                array_token[token_cnt++] = new_token;
                free(word);

                i += word_len;
                break;
            }
            default:
                i++;
                break;
        }



    }
    array_token[token_cnt++] = create_token(TOKEN_EOF, "EOF");

    return array_token;
}





/*
"||;dddfd&" - ||;dddfd&

||*dddfd& - || ; dddfd &

*/




/*

case bbrbr:
    char *word[5];  BNONONONONNONONONONO

case brbrbr: {
    char *word[5];

}




*/