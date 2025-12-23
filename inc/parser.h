#pragma once 
#include "token.h"
#include "ast.h"


ASTNode *parse(Token*);
ASTNode *parse_list(Token**);
ASTNode *parse_logical(Token**);
ASTNode *parse_pipeline(Token**);
ASTNode *parse_simple_command(Token**);
ASTNode *parse_factor(Token **);

int match(Token**, TokenType);


