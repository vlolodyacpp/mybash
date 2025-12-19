#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>


int execute(ASTNode*);

int execute_command(ASTNode*);
int execute_pipe(ASTNode*);






