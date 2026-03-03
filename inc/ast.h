#pragma once 
typedef enum {
    NODE_COMMAND, // def command
    NODE_PIPE,        // |
    NODE_PIPE_STDERR, // |&
    NODE_SEQUENCE,    // ;
    NODE_AND,         // &&
    NODE_OR,          // ||
    NODE_BACKGROUND,  // &
    NODE_GROUP,       // {}
    NODE_SUB,         // ()
} NodeType;



typedef enum {
    REDIR_IN,        // <
    REDIR_OUT,       // >
    REDIR_APPEND,    // >>
    REDIR_ERR_OUT,   // &>
    REDIR_ERR_APPEND, // &>>
} RedirType;



// перенаправлений в комманде несколько -> используем список для их обработки
typedef struct Redirection { 
    RedirType type;
    char  *filename;
    struct Redirection *next;
} Redirection;

typedef struct ASTNode { 
    NodeType type;
    union {

        // def command
        struct{
            char **argv;
            int *arg_no_expand;
            Redirection *redir;
            int argc;
        } command;
        // бинарные опператоры
        struct { 
            struct ASTNode *left;
            struct ASTNode *right;
        } binary;
        //унарные операторы
        struct {
            struct ASTNode *child;
        } unary;
    };

} ASTNode;


ASTNode *create_node(NodeType);
ASTNode *create_command(char**, int*, Redirection*, int);
ASTNode *create_binary(NodeType, ASTNode*, ASTNode*);
ASTNode *create_unary(NodeType, ASTNode*);

void add_redir(Redirection**, RedirType, const char*);
void free_redir(Redirection*);


void free_ast(ASTNode*);
void print_ast(ASTNode *);









