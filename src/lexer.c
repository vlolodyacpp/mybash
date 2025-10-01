#include "../inc/lexer.h"
#include "../inc/token.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MAX_TOKEN_LENGTH 256

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
    if(c == '\0') return END_LINE;
    if(c == '"' || c == '\'') return QUOTE;
    if(is_delimiter(c)) return DELIMETERS;
    return SIMPLE_WORD;
}

int define_word_len(char* word, int offset){
	int word_len = 0, delim_flag = 0;
	while (1) {
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

char* create_word(int word_len){
    char* new_word[word_len];
    return new_word;
}


Token *create_token(TokenType type, const char *value){
    Token *token = malloc(sizeof(Token));
    if(!token){
        return NULL;
    }

    token -> type = type;
    token -> value = value;

    return token;
}



Token *tokenize(const char *input){
    Token *array_token = (Token*)malloc(strlen(input) * sizeof(Token*));
    int i = 0;
    while (input[i] != '\0'){
        while(is_sace(input[i])) ++i;
        Token *new_token;

        SimpleWord type = define_simple_word(input[i]);
        switch(type){
            case DELIMETERS:
                if(input[i] == '|' && input[i+1] == '|'){
                    new_token = create_token(TOKEN_OR, "||");
                    i += 2;
                } else if(input[i] == '|' && input[i+1] != '|'){
                    new_token = create_token(TOKEN_PIPE, "|");
                    ++i;
                }

                if(input[i] == '>' && input[i+1] == '>'){
                    new_token = create_token(TOKEN_REDIR_APPEND, ">>");
                    i += 2;
                } else if(input[i] == '>' && input[i+1] != '>'){
                    new_token = create_token(TOKEN_REDIR_IN, ">");
                    ++i;
                }

                if(input[i] == '&' && input[i+1] == '&'){
                    new_token = create_token(TOKEN_AND, "&&");
                    i += 2;
                } else if(input[i] == '&' && input[i+1] != '&'){
                    new_token = create_token(TOKEN_AMPER, "&");
                    ++i;
                }
                if(input[i] == '<'){
                    new_token = create_token(TOKEN_REDIR_OUT, "<");
                    ++i;
                }
                if(input[i] == ';'){
                    new_token = create_token(TOKEN_REDIR_OUT, "<");
                    ++i;
                }
            case QUOTE:
                if(input[i] == '"'){
                    new_token = create_token(TOKEN_QUOTE, '"');
                } else {
                    new_token = create_token(TOKEN_QUOTE, '\'');
                }
            case SIMPLE_WORD:
                int word_len = define_word_len(input, i);
                char *word = create_word(word_len);
                for(int i = 0; i < word_len; ++i){
                    word[i] == input[i];
                }
                new_token = create_token(TOKEN_WORD, word);
            case END_LINE:



        }






    

        // ls cd pwd help echo exit jobs fg bg kill






    }
}