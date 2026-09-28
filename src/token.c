//
// Created by amelia on 9/27/26.
//

#include "token.h"

const char* TokenTypeToString(const TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "TOKEN_EOF";
        case TOKEN_ERROR: return "TOKEN_ERROR";
        case TOKEN_IDENT: return "TOKEN_IDENT";
        case TOKEN_INT_LIT: return "TOKEN_INT_LIT";
        case TOKEN_FLOAT_LIT: return "TOKEN_FLOAT_LIT";
        case TOKEN_STRING_LIT: return "TOKEN_STRING_LIT";
        case TOKEN_BOOL_LIT: return "TOKEN_BOOL_LIT";

        // Keywords
        case TOKEN_KEYWORD_TYPE: return "TOKEN_KEYWORD_TYPE";
        case TOKEN_KEYWORD_BOOL: return "TOKEN_KEYWORD_BOOL";
        case TOKEN_KEYWORD_I8:   return "TOKEN_KEYWORD_I8";
        case TOKEN_KEYWORD_I16:  return "TOKEN_KEYWORD_I16";
        case TOKEN_KEYWORD_I32:  return "TOKEN_KEYWORD_I32";
        case TOKEN_KEYWORD_I64:  return "TOKEN_KEYWORD_I64";
        case TOKEN_KEYWORD_U8:   return "TOKEN_KEYWORD_U8";
        case TOKEN_KEYWORD_U16:  return "TOKEN_KEYWORD_U16";
        case TOKEN_KEYWORD_U32:  return "TOKEN_KEYWORD_U32";
        case TOKEN_KEYWORD_U64:  return "TOKEN_KEYWORD_U64";
        case TOKEN_KEYWORD_F32:  return "TOKEN_KEYWORD_F32";
        case TOKEN_KEYWORD_F64:  return "TOKEN_KEYWORD_F64";

        // Syntax & Punctuation
        case TOKEN_COLON_COLON: return "TOKEN_COLON_COLON";
        case TOKEN_ARROW: return "TOKEN_ARROW";
        case TOKEN_FAT_ARROW: return "TOKEN_FAT_ARROW";
        case TOKEN_EQUAL: return "TOKEN_EQUAL";
        case TOKEN_DOT_LBRACE: return "TOKEN_DOT_LBRACE";
        case TOKEN_DOT: return "TOKEN_DOT";
        case TOKEN_LBRACE: return "TOKEN_LBRACE";
        case TOKEN_RBRACE: return "TOKEN_RBRACE";
        case TOKEN_LPAREN: return "TOKEN_LPAREN";
        case TOKEN_RPAREN: return "TOKEN_RPAREN";
        case TOKEN_COMMA: return "TOKEN_COMMA";
        case TOKEN_BACKTICK: return "TOKEN_BACKTICK";

        // Operators
        case TOKEN_PLUS: return "TOKEN_PLUS";
        case TOKEN_MINUS: return "TOKEN_MINUS";
        case TOKEN_STAR: return "TOKEN_STAR";
        case TOKEN_SLASH: return "TOKEN_SLASH";
        case TOKEN_EQ_EQ: return "TOKEN_EQ_EQ";
        case TOKEN_BANG_EQ: return "TOKEN_BANG_EQ";
        case TOKEN_LT: return "TOKEN_LT";
        case TOKEN_LTE: return "TOKEN_LTE";
        case TOKEN_GT: return "TOKEN_GT";
        case TOKEN_GTE: return "TOKEN_GTE";

        default: return "UNKNOWN";
    }
}
