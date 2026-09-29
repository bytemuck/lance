//
// Created by amelia on 9/27/26.
//

#ifndef LANCE_PARSER_H
#define LANCE_PARSER_H

#include "ast.h"
#include "lexer.h"

typedef struct {
    Lexer *lexer;
    Arena* arena; // Caller-owned; must outlive the parsed module and all its users
    Token current;
    Token peek;

    const char* fileName; // Used for diagnostics and recorded on every declaration

    bool hadError;
    bool panicMode; // Suppresses cascading errors until the next declaration
} Parser;

void InitializeParser(Parser* parser, Lexer *lexer, const char* fileName, Arena* arena);
// The returned module, its declarations, and partial parses belong to parser->arena.
AstModule* ParseModule(Parser* parser);

#endif //LANCE_PARSER_H
