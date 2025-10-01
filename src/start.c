#include <stdio.h>
#include "../inc/lexer.h"
#include "../inc/token.h"

int main(){
    char *vvod = "echo ыфваыва ";
    print(tokenize(vvod));
}