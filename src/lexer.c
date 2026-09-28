//
// Created by amelia on 9/27/26.
//

#include "lexer.h"

#include <stdio.h>
#include <string.h>

void InitializeLexer(Lexer* lexer, const char* source) {
    lexer->source = source;
    lexer->begin = 0;
    lexer->end = 0;
    lexer->line = 1;
    lexer->column = 0;
}

static void SkipNoise(Lexer* lexer) {
    for (;;) {
        const char c = lexer->source[lexer->end];
        if (c == '\0') {
            lexer->begin = lexer->end;
            break;
        }
        if (c == ' ' || c == '\t' || c == '\r') {
            lexer->end++;
            lexer->column++;
        } else if (c == '\n') {
            lexer->end++;
            lexer->line++;
            lexer->column = 0;
        } else if (c == '-' && lexer->source[lexer->end + 1] == '-') {
            lexer->end += 2;
            lexer->column += 2;
            while (lexer->source[lexer->end] != '\n' && lexer->source[lexer->end] != '\0') {
                lexer->end++;
                lexer->column++;
            }
        } else {
            lexer->begin = lexer->end;
            break;
        }
    }
}

static char Peek(const Lexer* lexer) {
    return lexer->source[lexer->end];
}

static char PeekNext(const Lexer* lexer) {
    return lexer->source[lexer->end + 1];
}

static char PeekN(const Lexer* lexer, const uint32_t n) {
    return lexer->source[lexer->end + n];
}

static char Advance(Lexer* lexer) {
    lexer->column++;

    if (lexer->source[lexer->end] == '\n') {
        lexer->line++;
        lexer->column = 0;
    }

    return lexer->source[lexer->end++];
}

static bool IsEOF(const Lexer* lexer) {
    return lexer->source[lexer->end] == '\0';
}

static bool IsAlpha(const char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool IsNumeric(const char c) {
    return c >= '0' && c <= '9';
}

static bool IsAlphaNumeric(const char c) {
    return IsAlpha(c) || IsNumeric(c);
}

static bool Match(Lexer* lexer, const char expected) {
    if (IsEOF(lexer) || lexer->source[lexer->end] != expected) {
        return false;
    }

    lexer->end++;
    lexer->column++;

    return true;
}

static bool MatchN(const Lexer* lexer, const char* expected, const uint32_t length) {
    const uint32_t expectedLength = strlen(expected);
    return length == expectedLength && strncmp(lexer->source + lexer->begin, expected, expectedLength) == 0;
}

static Token MakeToken(const Lexer* lexer, const TokenType type) {
    return (Token) {
        .type = type,
        .begin = lexer->begin,
        .end = lexer->end,
        .line = lexer->line,
        .column = lexer->column - (lexer->end - lexer->begin),
    };
}

static Token LexIdentifier(Lexer* lexer) {
    for (char c = Peek(lexer); IsAlphaNumeric(c); Advance(lexer), c = Peek(lexer)) {}

    const uint32_t length = lexer->end - lexer->begin;

    if (MatchN(lexer, "type", length)) {
        return MakeToken(lexer, TOKEN_KEYWORD_TYPE);
    }

    if (MatchN(lexer, "i8", length)) {
        return MakeToken(lexer, TOKEN_KEYWORD_I8);
    }

    if (MatchN(lexer, "i16", length)) {
        return MakeToken(lexer, TOKEN_KEYWORD_I16);
    }

    if (MatchN(lexer, "i32", length)) {
        return MakeToken(lexer, TOKEN_KEYWORD_I32);
    }

    if (MatchN(lexer, "i64", length)) {
        return MakeToken(lexer, TOKEN_KEYWORD_I64);
    }

    if (MatchN(lexer, "u8", length)) {
        return MakeToken(lexer, TOKEN_KEYWORD_U8);
    }

    if (MatchN(lexer, "u16", length)) {
        return MakeToken(lexer, TOKEN_KEYWORD_U16);
    }

    if (MatchN(lexer, "u32", length)) {
        return MakeToken(lexer, TOKEN_KEYWORD_U32);
    }

    if (MatchN(lexer, "u64", length)) {
        return MakeToken(lexer, TOKEN_KEYWORD_U64);
    }

    if (MatchN(lexer, "f32", length)) {
        return MakeToken(lexer, TOKEN_KEYWORD_F32);
    }

    if (MatchN(lexer, "f64", length)) {
        return MakeToken(lexer, TOKEN_KEYWORD_F64);
    }

    if (MatchN(lexer, "bool", length)) {
        return MakeToken(lexer, TOKEN_KEYWORD_BOOL);
    }

    if (MatchN(lexer, "string", length)) {
        return MakeToken(lexer, TOKEN_KEYWORD_STRING);
    }

    if (MatchN(lexer, "true", length)) {
        return MakeToken(lexer, TOKEN_BOOL_LIT);
    }

    if (MatchN(lexer, "false", length)) {
        return MakeToken(lexer, TOKEN_BOOL_LIT);
    }

    return MakeToken(lexer, TOKEN_IDENT);
}

static Token LexNumber(Lexer* lexer) {
    for (char c = Peek(lexer); IsNumeric(c); Advance(lexer), c = Peek(lexer)) {}

    const char p = Peek(lexer);
    const char pn = PeekNext(lexer);

    if (p == '.' && IsNumeric(pn)) {
        Advance(lexer);

        for (char c = Peek(lexer); IsNumeric(c); Advance(lexer), c = Peek(lexer)) {}
        return MakeToken(lexer, TOKEN_FLOAT_LIT);
    }

    return MakeToken(lexer, TOKEN_INT_LIT);
}

static Token LexString(Lexer* lexer) {
    for (char c = Peek(lexer); c != '"'; Advance(lexer), c = Peek(lexer)) {}

    if (IsEOF(lexer)) {
        return MakeToken(lexer, TOKEN_ERROR);
    }

    Advance(lexer);
    return MakeToken(lexer, TOKEN_STRING_LIT);
}

Token LexToken(Lexer* lexer) {
    SkipNoise(lexer);

    lexer->begin = lexer->end;

    if (IsEOF(lexer)) {
        return MakeToken(lexer, TOKEN_EOF);
    }

    const char c = Advance(lexer);

    if (IsAlpha(c)) {
        return LexIdentifier(lexer);
    }

    if (IsNumeric(c)) {
        return LexNumber(lexer);
    }

    if (c == '"') {
        return LexString(lexer);
    }

    switch (c) {
        case '(': return MakeToken(lexer, TOKEN_LPAREN);
        case ')': return MakeToken(lexer, TOKEN_RPAREN);
        case '{': return MakeToken(lexer, TOKEN_LBRACE);
        case '}': return MakeToken(lexer, TOKEN_RBRACE);
        case ',': return MakeToken(lexer, TOKEN_COMMA);
        case '`': return MakeToken(lexer, TOKEN_BACKTICK);
        case '+': return MakeToken(lexer, TOKEN_PLUS);
        case '*': return MakeToken(lexer, TOKEN_STAR);
        case '/': return MakeToken(lexer, TOKEN_SLASH);

        case '@': {
            while (IsAlphaNumeric(Peek(lexer))) {
                Advance(lexer);
            }

            return MakeToken(lexer, TOKEN_IDENT);
        }

        case ':':
            if (Match(lexer, ':')) return MakeToken(lexer, TOKEN_COLON_COLON);
            return MakeToken(lexer, TOKEN_ERROR);

        case '-':
            if (Match(lexer, '>')) return MakeToken(lexer, TOKEN_ARROW);
            return MakeToken(lexer, TOKEN_MINUS);

        case '=':
            if (Match(lexer, '=')) return MakeToken(lexer, TOKEN_EQ_EQ);
            if (Match(lexer, '>')) return MakeToken(lexer, TOKEN_FAT_ARROW);
            return MakeToken(lexer, TOKEN_EQUAL);

        case '!':
            if (Match(lexer, '=')) return MakeToken(lexer, TOKEN_BANG_EQ);
            return MakeToken(lexer, TOKEN_ERROR);

        case '<':
            if (Match(lexer, '=')) return MakeToken(lexer, TOKEN_LTE);
            return MakeToken(lexer, TOKEN_LT);

        case '>':
            if (Match(lexer, '=')) return MakeToken(lexer, TOKEN_GTE);
            return MakeToken(lexer, TOKEN_GT);

        case '.':
            if (Match(lexer, '{')) return MakeToken(lexer, TOKEN_DOT_LBRACE);
            return MakeToken(lexer, TOKEN_DOT);

        default:
            return MakeToken(lexer, TOKEN_ERROR);
    }

    return MakeToken(lexer, TOKEN_ERROR);
}