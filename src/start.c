#include <stdio.h>
#include "../inc/lexer.h"
#include "../inc/token.h"

int main(){
    char *vvod = "ls |& cat > file.txt 'a'";
    print(tokenize(vvod));
}