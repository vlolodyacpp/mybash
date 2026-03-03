#pragma once

#include "execution.h"
#include "jobs.h"
#include "ast.h"
#include <sys/types.h>



int handle_redirection(Redirection *);
void expand_argv(char **, int *);
int exec_command_in_child(ASTNode *);
int wait_foreground_pgid(pid_t, pid_t);

int execute_pipeline_node(ASTNode*);
int execute_internal(ASTNode*, int);
int execute_command(ASTNode *);
int execute(ASTNode *);
