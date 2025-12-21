#include "../inc/execution.h"
#include <signal.h>


int execute(ASTNode *node){
    if (!node) return 1;

    switch (node -> type){
        case NODE_COMMAND:
            return execute_command(node);
        case NODE_SEQUENCE:
            execute(node -> binary.left);
            return execute(node -> binary.right);
        case NODE_AND:
            if (execute(node -> binary.left) == 0) {
                return execute(node -> binary.right);
            }
            return 1;
        case NODE_OR:
            if (execute(node -> binary.left) != 0) {
                return execute(node -> binary.right);
            }
            return 0;    
        case NODE_PIPE:
            return execute_pipe(node);
        
        case NODE_BACKGROUND:

        default:
            fprintf(stderr, "Unknowm node type\n");
            return 1;
    }


}


int execute_command(ASTNode *node){
    char **argv = node -> command.argv;

    if(strcmp(argv[0], "cd") == 0){
        const char *path = argv[1];
        if (!path){
            path = getenv("HOME"); // cd без аргументов -> идем домой
        }
        if (chdir(path) != 0){
            perror("cd failed");
            return 1;
        }
        return 0;

    }

    if (strcmp(argv[0], "exit") == 0){
        exit(0);
    }

    if (strcmp(argv[0], "help") == 0){
        printf("Welcome to my bash: my command - cd, exit, help\n");
        return 0;
    }

    pid_t pid = fork();

    if (pid == 0){

        if (handle_redirection(node -> command.redir) != 0){
            exit(1);
        }

        execvp(argv[0], argv);
        fprintf(stderr, "%s: command not found\n", argv[0]);
        exit(1);

    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) return WEXITSTATUS(status);
        return 1;
    } else {
        perror("fork failed");
        return 1;
    }

}



int execute_pipe(ASTNode *node){
    int pipefd[2];
    pid_t pid_left, pid_right;

    // Создали трубу 
    // pipefd[0] - чтение
    // pipefd[1] - запись 

    if (pipe(pipefd) == -1){
        perror("pipe failed");
        return 1;
    }

    pid_left = fork();
    if (pid_left < 0){
        perror("fork left fail");
        return 1;
    }

    if (pid_left == 0) { 
        close(pipefd[0]);
        if(dup2(pipefd[1], STDOUT_FILENO) == -1){
            perror("dup2 left fail");
            return 1;
        }
        close(pipefd[1]);

        exit(execute(node -> binary.left));
    }

    pid_right = fork();
    if (pid_right < 0) {
        perror("fork right fail");

        close(pipefd[0]);
        close(pipefd[1]);
        kill(pid_left, SIGTERM);

        return 1;
    }
    if (pid_right == 0) { 
        close(pipefd[1]);

        if(dup2(pipefd[0], STDIN_FILENO) == -1){
            perror("dup2 right failed");
            exit(1);

        }
        close(pipefd[0]);

        exit(execute(node -> binary.right));
    }

    close(pipefd[0]);
    close(pipefd[1]);

    int status_left, status_right;
    waitpid(pid_left, &status_left, 0);
    waitpid(pid_right, &status_right, 0);

    if (WIFEXITED(status_right)) return WEXITSTATUS(status_right);
    return 1;


}

int handle_redirection(Redirection *redir){
    while(redir){
        int fd;
        if (redir -> type == REDIR_IN){
            fd = open(redir -> filename, O_RDONLY);
            if(fd < 0) {
                perror(redir -> filename);
                return 1;
            }
            dup2(fd, STDIN_FILENO);

        } else if (redir -> type == REDIR_OUT){
            fd = open(redir -> filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(fd < 0) {
                perror(redir -> filename);
                return 1;
            }
            dup2(fd, STDOUT_FILENO);
        } else if (redir -> type == REDIR_APPEND){
            fd = open(redir -> filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(fd < 0) {
                perror(redir -> filename);
                return 1;
            }
            dup2(fd, STDOUT_FILENO);
        }

        close(fd);
        redir = redir -> next;
    }

    return 0;
}

