#pragma once 

#include "ast.h"

int is_builtin(const char *);

int builtin_cd(char **argv);
int builtin_exit(char **argv);
int builtin_pwd(char **argv);
int builtin_echo(char **argv);
int builtin_help(char **argv);
int builtin_jobs(char **argv);
int builtin_kill(char **argv);
int builtin_fg(char **argv);
int builtin_bg(char **argv);
int builtin_set(char **argv);
int builtin_unset(char **argv);

int run_builtin(char **);
int run_builtin_with_redir(ASTNode *);

