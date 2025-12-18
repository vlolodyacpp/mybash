#include <stdio.h>
#include "../inc/lexer.h"
#include "../inc/token.h"

int main(){
    char *vvod = "ls|& cat >>& ;| |& & &>> &> file.txt '' &> sdfsd sdfsd sfsdf sdsdfsd sdfsd sdfsdf sdfsd sfsd fsdfsd fsd sdfsd ";
    Token *array = tokenize(vvod);
    print(array);
    free_tokens(array);
}
