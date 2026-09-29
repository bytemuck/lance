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

    const char* fileName; // Used for diagnostics and recorded on every declaration

    bool hadError;
    bool panicMode; // Suppresses cascading errors until the next declaration
} Parser;

void InitializeParser(Parser* parser, Lexer *lexer, const char* fileName);
AstModule* ParseModule(Parser* parser);

#endif //LANCE_PARSER_H
