#include "../inc/execution.h"


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

    pid_t pid = fork();

    if (pid == 0){
        execvp(argv[0], argv);

        perror("exec fail");
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
    if (!node) return 1;
    
    return 0;
}


