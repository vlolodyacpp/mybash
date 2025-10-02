
#include "../inc/lexer.h"
#include "../inc/token.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

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
        int delim_flag = 0;
		for (int i = 0; i < COUNT_DELIMITERS; i++){
			if (word[offset] == word_delimeters[i]){
				delim_flag = 1;
				break;
			}
        }
        if (delim_flag) break;
		if (word[offset] == '\0') break;
		word_len++;
		offset++;
	}
	return word_len;
}

void free_tokens(Token **array_token) {
    if (!array_token) return;
    
    for (int i = 0; array_token[i] != NULL; i++) {
        if (array_token[i]->value) {
            free(array_token[i]->value);
        }
        free(array_token[i]);
    }
    free(array_token);
}


void print(Token **array) {
    printf("=== TOKENS ===:\n");
    for (int i = 0; array[i] != NULL; ++i) {
        printf("%d - %s\n", array[i]->type, array[i]->value ? array[i]->value : "(null)");
    }
    printf("=== END ===\n");
}

Token *create_token(TokenType type, char *value){
    Token *token = malloc(sizeof(Token));
    if(!token){
        return NULL;
    }

    token -> type = type;
    if (value){
       token -> value = strdup(value);
    } else {
       token -> value = NULL;
    }

    return token;
}



Token **tokenize(const char *input){
    Token **array_token = (Token**)malloc((strlen(input) + 2) * sizeof(Token*));
    int i = 0, token_cnt = 0;
    while (input[i] != '\0'){
        while(is_space(input[i])) ++i;
        Token *new_token;

        SimpleWord type = define_simple_word(input[i]);
        switch(type){
            case DELIMETERS:
                if(input[i] == '|' && input[i+1] == '|'){
                    new_token = create_token(TOKEN_OR, "||");
                    i += 2;
                } else if(input[i] == '|'){
                    new_token = create_token(TOKEN_PIPE, "|");
                    i++;
                } else if(input[i] == '>' && input[i+1] == '>'){
                    new_token = create_token(TOKEN_REDIR_APPEND, ">>");
                    i += 2;
                } else if(input[i] == '>'){
                    new_token = create_token(TOKEN_REDIR_IN, ">");
                    i++;
                } else if(input[i] == '&' && input[i+1] == '&'){
                    new_token = create_token(TOKEN_AND, "&&");
                    i += 2;
                } else if(input[i] == '&'){
                    new_token = create_token(TOKEN_AMPER, "&");
                    i++;
                } else if(input[i] == '<'){
                    new_token = create_token(TOKEN_REDIR_OUT, "<");
                    i++;
                } else if(input[i] == ';'){
                    new_token = create_token(TOKEN_SEMICOL, ";");
                    i++;
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
    array_token[token_cnt] = NULL; 

    return array_token;
}










/*

case bbrbr:
    char *word[5];  BNONONONONNONONONONO

case brbrbr: {
    char *word[5];

}




*/