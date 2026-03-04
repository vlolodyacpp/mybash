#pragma once 

#include "ast.h"
#include <signal.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

int is_builtin(const char *);

int builtin_cd(char **);
int builtin_exit(char **);
int builtin_pwd(char **);
int builtin_echo(char **);
int builtin_help(char **);
int builtin_jobs(char **);
int builtin_kill(char **);
int builtin_fg(char **);
int builtin_bg(char **);
int builtin_history(char **);
int is_valid_var_name(const char *, size_t);
int builtin_set(char **);
int builtin_unset(char **);

void request_shell_exit(int code);
int shell_exit_requested(void);
int shell_exit_code(void);

int run_builtin(char **);
int run_builtin_with_redir(ASTNode *);

