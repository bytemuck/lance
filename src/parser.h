//
// Created by amelia on 9/27/26.
//

#ifndef LANCE_PARSER_H
#define LANCE_PARSER_H

#include "ast.h"
#include "lexer.h"

typedef struct {
    Lexer *lexer;
    Token current;
    Token peek;

    bool hadError;
} Parser;

void InitializeParser(Parser* parser, Lexer *lexer);
AstModule* ParseModule(Parser* parser);

#endif //LANCE_PARSER_H
