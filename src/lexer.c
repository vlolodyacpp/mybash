
#include "../inc/lexer.h"
#include "../inc/token.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MAX_TOKEN_LENGTH 256
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
    if(c == '\0') return END_LINE;
    if(c == '"' || c == '\'') return WORD_IN_QUOTES;
    if(is_delimiter(c)) return DELIMETERS;
    return SIMPLE_WORD;
}

int define_word_len(const char* word, int offset){
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

Token *create_token(TokenType type, char *value){
    Token *token = malloc(sizeof(Token));
    if(!token){
        return NULL;
    }

    token -> type = type;
    token -> value = value;

    return token;
}



Token **tokenize(const char *input){
    Token **array_token = (Token**)malloc(strlen(input) * sizeof(Token*));
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
                } else if(input[i++] == '|'){
                    new_token = create_token(TOKEN_PIPE, "|");
                }

                if(input[i] == '>' && input[i+1] == '>'){
                    new_token = create_token(TOKEN_REDIR_APPEND, ">>");
                    i += 2;
                } else if(input[i++] == '>'){
                    new_token = create_token(TOKEN_REDIR_IN, ">");
                }

                if(input[i] == '&' && input[i+1] == '&'){
                    new_token = create_token(TOKEN_AND, "&&");
                    i += 2;
                } else if(input[i++] == '&'){
                    new_token = create_token(TOKEN_AMPER, "&");
                }
                if(input[i++] == '<'){
                    new_token = create_token(TOKEN_REDIR_OUT, "<");
                }
                if(input[i++] == ';'){
                    new_token = create_token(TOKEN_REDIR_OUT, "<");
                }
                array_token[token_cnt++] = new_token;
                break;
            case WORD_IN_QUOTES: {
                char q = '\'';
                if(input[i] == '"'){
                    q = '"';
                }

                int count = 1;
                for( ; input[i + count] != q && input[i + count] != '\0'; ++count); // len_word in "" or ''
                char word[count + 1]; // + размер слова

                strncpy(word, &(input[i]), count);
                word[count] = '\0';
                i += (count + 1);
                new_token = create_token(TOKEN_WORD_IN_QUOTES, word);
                array_token[token_cnt++] = new_token;
                break;


                // echo "asdfsdafsdaf"
                // echo
                // "asdffsdsdfasdfa"
            }
            case SIMPLE_WORD: {
                int word_len = define_word_len(input, i);
                char word[word_len];
                for(int i = 0; i < word_len; ++i){
                    word[i] = input[i];
                }
                new_token = create_token(TOKEN_WORD, word);
                array_token[token_cnt] = new_token;
                i += word_len;
                break;
            }
            default:
                break;
        }



    }

    return array_token;
}


void print(Token **array) {
    printf("    TOKENS:\n");
    for (int i = 0; array[i] && array[i]->type != TOKEN_EOF; ++i) {
        printf("%d - %s\n", array[i]->type, array[i]->value ? array[i]->value : "(null)");
    }
    printf("=== END ===\n");
}






/*

case bbrbr:
    char *word[5];  BNONONONONNONONONONO

case brbrbr: {
    char *word[5];

}




*/