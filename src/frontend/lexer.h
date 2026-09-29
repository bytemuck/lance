//
// Created by amelia on 9/27/26.
//

#ifndef LANCE_LEXER_H
#define LANCE_LEXER_H

#include "token.h"

typedef struct {
    const char* source;
    uint32_t begin; // The start of the token currently being tokenized
    uint32_t end; // The current position in the source

    uint32_t line;
    uint32_t column;

    // Position of `begin`, used for the location of the token being lexed
    uint32_t beginLine;
    uint32_t beginColumn;
} Lexer;

void InitializeLexer(Lexer* lexer, const char* source);

Token LexToken(Lexer* lexer);

#endif //LANCE_LEXER_H
