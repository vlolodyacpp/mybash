#pragma once

#include "execution.h"
#include "jobs.h"
#include "ast.h"
#include <sys/types.h>



const char *job_status_str(JobStatus st);

int builtin_kill(char **);
int builtin_set(char **);
int builtin_unset(char **);
int run_builtin(char **);

int run_builtin_with_redir(ASTNode *);
void expand_argv(char **);

int exec_command_in_child(ASTNode *);

int flatten_pipeline(ASTNode *,ASTNode ***, int **, int *);

int wait_foreground_pgid(pid_t, pid_t);

int execute_pipeline_node(ASTNode*);

int execute_internal(ASTNode*, int);
int execute(ASTNode *node);
int execute_command(ASTNode *node);
