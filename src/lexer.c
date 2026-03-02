#include "../inc/lexer.h"
#include "../inc/token.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define STANDART_CAPACITY 16


const char word_delimeters[] = " \t;|&<>()"; 
int g_unclosed_quote = 0;

typedef struct {
    const char *lexeme;
    TokenType type;
} OperatorDef;

int match_greedy_operator(const char *input, size_t pos, size_t len, Token *out_token, size_t *matched_len) {
    static const OperatorDef ops[] = {
        {"&>>", TOKEN_AMPER_REDIR_APPEND},
        {"|&", TOKEN_PIPE_AMPER},
        {"||", TOKEN_OR},
        {"&&", TOKEN_AND},
        {">>", TOKEN_REDIR_APPEND},
        {"&>", TOKEN_AMPER_REDIR_IN},
        {"|", TOKEN_PIPE},
        {"&", TOKEN_AMPER},
        {"<", TOKEN_REDIR_IN},
        {">", TOKEN_REDIR_OUT},
        {";", TOKEN_SEMICOL},
        {"(", TOKEN_LPAREN},
        {")", TOKEN_RPAREN},
    };

    size_t best_idx = (size_t)-1;
    size_t best_len = 0;
    size_t ops_count = sizeof(ops) / sizeof(ops[0]);

    for (size_t op_idx = 0; op_idx < ops_count; ++op_idx) {
        size_t op_len = strlen(ops[op_idx].lexeme);
        if (pos + op_len > len) {
            continue;
        }
        if (strncmp(input + pos, ops[op_idx].lexeme, op_len) == 0 && op_len > best_len) {
            best_idx = op_idx;
            best_len = op_len;
        }
    }

    if (best_idx == (size_t)-1) {
        return 0;
    }

    *out_token = create_token(ops[best_idx].type, (char *)ops[best_idx].lexeme);
    *matched_len = best_len;
    return 1;
}

int is_delimiter(char c){
    for(int i = 0; i < COUNT_DELIMITERS; ++i){
        if(c == word_delimeters[i]) return 1;
    }
    return 0;
}


int is_space(char c){
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
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
    for (i = 0; array_token[i].type != TOKEN_EOF && array_token[i].value != NULL; i++) {
        if (array_token[i].value) { 
            free(array_token[i].value);
        }
    }
    free(array_token);
}


void print_tokens(Token *array_token) {
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

    g_unclosed_quote = 0;

    size_t i = 0;
    while (i < len){
        while(is_space(input[i])) ++i;

        if(input[i] == '#') break;

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
                {
                    size_t op_len = 0;
                    if (match_greedy_operator(input, i, len, &new_token, &op_len)) {
                        array_token[token_cnt++] = new_token;
                        i += op_len;
                    } else {
                        i++;
                    }
                }
                break;          

            case WORD_IN_QUOTES:
            case SIMPLE_WORD: {
                size_t j = i;
                size_t word_cap = 32;
                size_t out = 0;
                int has_single_quotes = 0;
                char *word = (char*)malloc(word_cap);
                if (!word) {
                    perror("malloc error");
                    free_tokens(array_token);
                    return NULL;
                }

                while (j < len && !is_space(input[j]) && !is_delimiter(input[j])) {
                    if (input[j] == '"' || input[j] == '\'') {
                        char q = input[j++];
                        if (q == '\'') {
                            has_single_quotes = 1;
                        }
                        int closed = 0;

                        while (j < len) {
                            if (input[j] == '\\') {
                                if (j + 1 >= len) {
                                    g_unclosed_quote = 1;
                                    free(word);
                                    array_token[token_cnt].type = TOKEN_EOF;
                                    array_token[token_cnt].value = NULL;
                                    free_tokens(array_token);
                                    return NULL;
                                }

                                if (out + 2 > word_cap) {
                                    word_cap *= 2;
                                    char *new_word = (char*)realloc(word, word_cap);
                                    if (!new_word) {
                                        perror("realloc error");
                                        free(word);
                                        free_tokens(array_token);
                                        return NULL;
                                    }
                                    word = new_word;
                                }

                                word[out++] = input[j + 1];
                                j += 2;
                                continue;
                            }

                            if (input[j] == q) {
                                closed = 1;
                                j++;
                                break;
                            }

                            if (out + 2 > word_cap) {
                                word_cap *= 2;
                                char *new_word = (char*)realloc(word, word_cap);
                                if (!new_word) {
                                    perror("realloc error");
                                    free(word);
                                    free_tokens(array_token);
                                    return NULL;
                                }
                                word = new_word;
                            }
                            word[out++] = input[j++];
                        }

                        if (!closed) {
                            g_unclosed_quote = 1;
                            free(word);
                            array_token[token_cnt].type = TOKEN_EOF;
                            array_token[token_cnt].value = NULL;
                            free_tokens(array_token);
                            return NULL;
                        }

                        continue;
                    }

                    if (input[j] == '\\') {
                        if (j + 1 >= len) {
                            fprintf(stderr, "syntax error: trailing \n");
                            free(word);
                            free_tokens(array_token);
                            return NULL;
                        }

                        if (out + 2 > word_cap) {
                            word_cap *= 2;
                            char *new_word = (char*)realloc(word, word_cap);
                            if (!new_word) {
                                perror("realloc error");
                                free(word);
                                free_tokens(array_token);
                                return NULL;
                            }
                            word = new_word;
                        }

                        word[out++] = input[j + 1];
                        j += 2;
                        continue;
                    }

                    if (out + 2 > word_cap) {
                        word_cap *= 2;
                        char *new_word = (char*)realloc(word, word_cap);
                        if (!new_word) {
                            perror("realloc error");
                            free(word);
                            free_tokens(array_token);
                            return NULL;
                        }
                        word = new_word;
                    }

                    word[out++] = input[j++];
                }

                word[out] = '\0';

                new_token = create_token(has_single_quotes ? TOKEN_WORD_IN_QUOTES : TOKEN_WORD, word);
                array_token[token_cnt++] = new_token;
                free(word);

                i = j;
                break;
            }
            default:
                i++;
                break;
        }



    }
    array_token[token_cnt].type = TOKEN_EOF;
    array_token[token_cnt].value = NULL;

    return array_token;
}


