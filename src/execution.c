#include "../inc/execution.h"
#include "../inc/jobs.h"
#include <signal.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>


extern Job *first_job;
extern int shell_terminal;
extern int shell_is_interactive;
extern pid_t shell_pgid;

void print_jobs_list() {
    Job *job = first_job;
    while (job) {
        const char *status_str = (job->status == JOB_STOPPED) ? "Stopped" : "Running";
        printf("[%d] %d %s %s\n", job->id, job->pgid, status_str, job->command);
        job = job->next;
    }
}

int execute(ASTNode *node) {
    if (!node) return 1;

    switch (node->type) {
        case NODE_COMMAND:
            return execute_command(node);

        case NODE_PIPE:
            return execute_pipe(node);
        case NODE_PIPE_STDERR:
            return execute_pipe_stderr(node);

        case NODE_SEQUENCE:
            execute(node->binary.left);
            return execute(node->binary.right);

        case NODE_AND:
            if (execute(node->binary.left) == 0) {
                return execute(node->binary.right);
            }
            return 1;

        case NODE_OR:
            if (execute(node->binary.left) != 0) {
                return execute(node->binary.right);
            }
            return 0;

        case NODE_BACKGROUND: {
           
            pid_t pid = fork();
            if (pid == 0) {
            
                setpgid(0, 0); 

                signal(SIGINT, SIG_IGN);
                signal(SIGQUIT, SIG_IGN);
                signal(SIGTSTP, SIG_IGN);
                
                exit(execute(node->unary.child));
            } else if (pid > 0) {
                setpgid(pid, pid); 
                
                char *cmd_name = "Background Process";
                if (node->unary.child->type == NODE_COMMAND) {
                    cmd_name = node->unary.child->command.argv[0];
                }

                add_job(pid, cmd_name, JOB_RUNNING, 1);
                Job *j = find_job_by_pgid(pid);
                if (j) printf("[%d] %d\n", j->id, pid);
                
                return 0; 
            } else {
                perror("fork background error");
                return 1;
            }
        }

        case NODE_SUB: {
            pid_t pid = fork();
            if (pid == 0) {
                exit(execute(node->unary.child));
            } else if (pid > 0) {
                int status;
                waitpid(pid, &status, 0);
                if (WIFEXITED(status)) return WEXITSTATUS(status);
                return 1;
            } else {
                perror("fork subshell error");
                return 1;
            }
        }
        
        case NODE_GROUP:
             return execute(node->unary.child);

        default:
            fprintf(stderr, "Unknown node type\n");
            return 1;
    }
}

int execute_command(ASTNode *node) {
    char **argv = node->command.argv;
    if (!argv || !argv[0]) return 0;


    if (strcmp(argv[0], "cd") == 0) {
        const char *path = argv[1] ? argv[1] : getenv("HOME");
        if(!path){
            fprintf(stderr, "cd: home not set");
            return 1;
        }
        if (chdir(path) != 0) {
            perror("cd failed");
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[0], "exit") == 0) exit(0);
    if (strcmp(argv[0], "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd);
        else perror("getcwd");
        return 0;
    }
    if (strcmp(argv[0], "echo") == 0) {
        for (int i = 1; argv[i]; ++i) printf("%s%s", argv[i], argv[i+1] ? " " : "");
        printf("\n");
        return 0;
    }
    if (strcmp(argv[0], "help") == 0) {
        printf("MyBash commands: cd, exit, help, pwd, echo, jobs, fg, bg\n");
        return 0;
    }

    if (strcmp(argv[0], "jobs") == 0) {
        print_jobs_list();
        return 0;
    }
    if (strcmp(argv[0], "fg") == 0) return builtin_fg(argv);
    if (strcmp(argv[0], "bg") == 0) return builtin_bg(argv);

    pid_t pid = fork();

    if (pid == 0) {
        setpgid(0, 0);

        if (shell_is_interactive) {
            tcsetpgrp(shell_terminal, getpid());
            
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
        }

        if (handle_redirection(node->command.redir) != 0) {
            exit(1);
        }

        execvp(argv[0], argv);
        fprintf(stderr, "%s: command not found\n", argv[0]);
        exit(127);

    } else if (pid > 0) {
        
        setpgid(pid, pid);

        if (shell_is_interactive) {
            add_job(pid, argv[0], JOB_RUNNING, 0);
            Job *job = find_job_by_pgid(pid);

            tcsetpgrp(shell_terminal, pid);

            wait_for_job(job);

            tcsetpgrp(shell_terminal, shell_pgid);
        } else {
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) return WEXITSTATUS(status);
            return 1;
        }
        return 0;
    } else {
        perror("fork failed");
        return 1;
    }
}

int execute_pipe(ASTNode *node) {
    int pipefd[2];
    pid_t pid_left, pid_right;

    if (pipe(pipefd) == -1) { perror("pipe failed"); return 1; }

    pid_left = fork();
    if (pid_left < 0) { perror("fork left"); return 1; }

    if (pid_left == 0) {
        setpgid(0, 0); 
        if (shell_is_interactive) {
            tcsetpgrp(shell_terminal, getpid());
            signal(SIGINT, SIG_DFL); signal(SIGQUIT, SIG_DFL); signal(SIGTSTP, SIG_DFL);
        }

        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        
        exit(execute(node->binary.left));
    }

    pid_right = fork();
    if (pid_right < 0) {
        perror("fork right");
        close(pipefd[0]); close(pipefd[1]);
        return 1;
    }

    if (pid_right == 0) {
        setpgid(0, pid_left); 
        if (shell_is_interactive) {
            tcsetpgrp(shell_terminal, pid_left);
            signal(SIGINT, SIG_DFL); signal(SIGQUIT, SIG_DFL); signal(SIGTSTP, SIG_DFL);
        }

        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);

        exit(execute(node->binary.right));
    }

    close(pipefd[0]);
    close(pipefd[1]);

    setpgid(pid_left, pid_left);
    setpgid(pid_right, pid_left);

    if (shell_is_interactive) {
        add_job(pid_left, "Pipe", JOB_RUNNING, 0);
        Job *job = find_job_by_pgid(pid_left);

        tcsetpgrp(shell_terminal, pid_left);
        wait_for_job(job); 
        tcsetpgrp(shell_terminal, shell_pgid);
    } else {
        waitpid(pid_left, NULL, 0);
        waitpid(pid_right, NULL, 0);
    }

    return 0;
}

int execute_pipe_stderr(ASTNode *node) {
    
    int pipefd[2];
    pid_t pid_left, pid_right;

    if (pipe(pipefd) == -1) return 1;

    pid_left = fork();
    if (pid_left == 0) {
        setpgid(0, 0);
        if (shell_is_interactive) {
            tcsetpgrp(shell_terminal, getpid());
            signal(SIGINT, SIG_DFL); signal(SIGQUIT, SIG_DFL); signal(SIGTSTP, SIG_DFL);
        }
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO); 
        close(pipefd[1]);
        exit(execute(node->binary.left));
    }

    pid_right = fork();
    if (pid_right == 0) {
        setpgid(0, pid_left);
        if (shell_is_interactive) {
            tcsetpgrp(shell_terminal, pid_left);
            signal(SIGINT, SIG_DFL); signal(SIGQUIT, SIG_DFL); signal(SIGTSTP, SIG_DFL);
        }
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        exit(execute(node->binary.right));
    }

    close(pipefd[0]); close(pipefd[1]);
    setpgid(pid_left, pid_left);
    setpgid(pid_right, pid_left);

    if (shell_is_interactive) {
        add_job(pid_left, "Pipe|&", JOB_RUNNING, 0);
        Job *job = find_job_by_pgid(pid_left);
        tcsetpgrp(shell_terminal, pid_left);
        wait_for_job(job);
        tcsetpgrp(shell_terminal, shell_pgid);
    } else {
        waitpid(pid_left, NULL, 0);
        waitpid(pid_right, NULL, 0);
    }
    return 0;
}

int handle_redirection(Redirection *redir) {
    while (redir) {
        int fd = -1;
        int flags = 0;
        int dest_fd = STDOUT_FILENO;

        switch (redir->type) {
            case REDIR_IN:
                fd = open(redir->filename, O_RDONLY);
                dest_fd = STDIN_FILENO;
                break;
            case REDIR_OUT:
                flags = O_WRONLY | O_CREAT | O_TRUNC;
                break;
            case REDIR_APPEND:
                flags = O_WRONLY | O_CREAT | O_APPEND;
                break;
            case REDIR_ERR_APPEND:
                flags = O_WRONLY | O_CREAT | O_APPEND;
                break;
            case REDIR_ERR_OUT:
                flags = O_WRONLY | O_CREAT | O_TRUNC;
                break;
        }

        if (redir->type != REDIR_IN) {
            fd = open(redir->filename, flags, 0644);
        }

        if (fd < 0) {
            perror(redir->filename);
            return 1;
        }

        if (redir->type == REDIR_ERR_APPEND || redir->type == REDIR_ERR_OUT) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
        } else {
            dup2(fd, dest_fd);
        }

        close(fd);
        redir = redir -> next;
    }
    return 0;
}