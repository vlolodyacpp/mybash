#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>


int execute(ASTNode*);

int handle_redirection(Redirection *);
int execute_command(ASTNode*);
int execute_pipe(ASTNode*);






