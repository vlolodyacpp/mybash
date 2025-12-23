#include "../inc/ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ASTNode *create_node(NodeType type){
    ASTNode *node = malloc(sizeof(ASTNode));
    if(!node){
        perror("malloc error");
        free(node);
        return NULL;
    }
    node -> type = type;
    return node;
}


ASTNode *create_command(char** argv, Redirection *redir, int argc){
    ASTNode *node = create_node(NODE_COMMAND);
    if(!node){
        perror("malloc error");
        free(node);
        return NULL;
    }

    node -> command.argc = argc;
    node -> command.argv = argv;
    node -> command.redir = redir;


    return node;
}

// Бинарные опператоры
ASTNode *create_binary(NodeType type, ASTNode *left, ASTNode *right){
    ASTNode *node = create_node(type);
    if(!node){
        perror("malloc error");
        free(node);
        return NULL;
    }

    node -> binary.left = left;
    node -> binary.right = right;

    return node;
}

 
ASTNode *create_unary(NodeType type, ASTNode *child){
    ASTNode *node = create_node(type);
    if(!node){
        perror("malloc error");
        free(node);
        return NULL;
    }

    node -> unary.child = child;

    return node;
}


void add_redir(Redirection **head, RedirType rtype, const char *file){

    Redirection *redir = malloc(sizeof(Redirection));
    if (!redir) return;

    redir -> type = rtype;
    redir -> filename = strdup(file);
    redir -> next = NULL;


    // no comment
    if(!*head) { 
        *head = redir;
    } else {
        Redirection *current = *head;
        while(current -> next) current = current -> next;
        current -> next = redir;
    }

}


//Вспомогательная для освобождение дерева

void free_redir(Redirection *redir){ 
    while(redir){
        Redirection *next = redir -> next;
        free(redir -> filename);
        free(redir);
        redir = next;
    }
}

// Удаляем дерево
void free_ast(ASTNode *node){

    if(!node) return;

    switch(node -> type) { 
        case NODE_COMMAND:
            for(int i = 0; i < node -> command.argc; ++i){
                free(node -> command.argv[i]);
            }
            free(node -> command.argv);
            free_redir(node -> command.redir);
            break;

        case NODE_PIPE:        
        case NODE_PIPE_STDERR:
        case NODE_SEQUENCE:    
        case NODE_AND:         
        case NODE_OR:
            free_ast(node -> binary.left);
            free_ast(node -> binary.right);
            break;
        case NODE_SUB: 
        case NODE_BACKGROUND:  // &
        case NODE_GROUP:  
            free_ast(node -> unary.child);
            break;     // {}
    }

    free(node);


}

const char *get_node_name(NodeType type) {
    switch (type) {
        case NODE_COMMAND:    return "COMMAND";
        case NODE_PIPE:       return "PIPE";
        case NODE_SEQUENCE:   return "SEQUENCE";
        case NODE_AND:        return "AND";
        case NODE_OR:         return "OR";
        case NODE_BACKGROUND: return "BACK";
        case NODE_SUB:        return "SUBSHELL";
        case NODE_GROUP:      return "GROUP";
        default:              return "UNKNOWN";
    }
}

const char *get_redir_name(RedirType rt) {
    switch (rt) {
        case REDIR_IN:          return "<";
        case REDIR_OUT:         return ">";
        case REDIR_APPEND:      return ">>";
        case REDIR_ERR_OUT:     return "&>";
        case REDIR_ERR_APPEND:  return "&>>";
        default:                return "?";
    }
}

void print_level(int level) {
    for (int i = 0; i < level; i++) {
        printf("  ");
    }
}

void print_tree(ASTNode *node, int level) {
    if (!node) return;
    
    print_level(level);
    printf("%s", get_node_name(node->type));
    
    switch (node->type) {
        case NODE_COMMAND:
            printf(" [");
            for (int i = 0; i < node->command.argc; i++) {
                printf("%s%s", node->command.argv[i], 
                       i < node->command.argc - 1 ? ", " : "");
            }
            printf("]");
            
            for (Redirection *redir = node->command.redir; redir; redir = redir->next) {
                printf(" %s %s", get_redir_name(redir->type), redir->filename);
            }
            printf("\n");
            break;
            
        case NODE_PIPE:
        case NODE_SEQUENCE:
        case NODE_AND:
        case NODE_PIPE_STDERR:
        case NODE_OR:
            printf("\n");
            print_tree(node->binary.left, level + 1);
            print_tree(node->binary.right, level + 1);
            break;
            
        case NODE_BACKGROUND:
        case NODE_SUB:
        case NODE_GROUP:
            printf("\n");
            print_tree(node->unary.child, level + 1);
            break;
    }
}

void print_ast(ASTNode *node) {
    printf("=== AST ===\n");
    print_tree(node, 0);
    printf("=== END ===\n");
}
