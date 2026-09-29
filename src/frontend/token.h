//
// Created by amelia on 9/27/26.
//

#ifndef LANCE_TOKEN_H
#define LANCE_TOKEN_H

#include <stdint.h>

typedef enum {
    TOKEN_EOF,
    TOKEN_ERROR,

    // Identifiers & Literals
    TOKEN_IDENT,
    TOKEN_INT_LIT,
    TOKEN_FLOAT_LIT,
    TOKEN_STRING_LIT,
    TOKEN_BOOL_LIT, // 'true', 'false'

    // Primitive Type Keywords
    TOKEN_KEYWORD_IMPL_MINIMUM,
    TOKEN_KEYWORD_TYPE, // 'Type'
    TOKEN_KEYWORD_BOOL, // 'bool'
    TOKEN_KEYWORD_STRING, // 'string'
    TOKEN_KEYWORD_I8,   // 'i8'
    TOKEN_KEYWORD_I16,  // 'i16'
    TOKEN_KEYWORD_I32,  // 'i32'
    TOKEN_KEYWORD_I64,  // 'i64'
    TOKEN_KEYWORD_U8,   // 'u8'
    TOKEN_KEYWORD_U16,  // 'u16'
    TOKEN_KEYWORD_U32,  // 'u32'
    TOKEN_KEYWORD_U64,  // 'u64'
    TOKEN_KEYWORD_F32,  // 'f32'
    TOKEN_KEYWORD_F64,  // 'f64'
    TOKEN_KEYWORD_IMPL_MAXIMUM,

    // Declaration & Expression Keywords
    TOKEN_IMPORT,       // 'import'
    TOKEN_LET,          // 'let'
    TOKEN_IN,           // 'in'
    TOKEN_LAZY,         // 'lazy', a parameter type modifier

    // Operators & Punctuation
    TOKEN_COLON_COLON,  // ::
    TOKEN_ARROW,        // ->
    TOKEN_FAT_ARROW,    // =>
    TOKEN_EQUAL,        // =
    TOKEN_DOT_LBRACE,   // .{
    TOKEN_DOT,          // .
    TOKEN_LBRACE,       // {
    TOKEN_RBRACE,       // }
    TOKEN_LPAREN,       // (
    TOKEN_RPAREN,       // )
    TOKEN_COMMA,        // ,
    TOKEN_BACKTICK,     // `

    // Prefix Operators (treated as callable symbols)
    TOKEN_OPERATOR_MINIMUM,
    TOKEN_OPERATOR_SYMBOL,
    TOKEN_OPERATOR_MAXIMUM,
} TokenType;

typedef struct {
    TokenType type;

    uint32_t begin;
    uint32_t end;

    uint32_t line;
    uint32_t column;

    // Human-readable reason, set only for TOKEN_ERROR.
    const char* message;
} Token;

const char* TokenTypeToString(TokenType type);

#endif //LANCE_TOKEN_H
