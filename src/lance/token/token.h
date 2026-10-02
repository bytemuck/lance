#pragma once

#include "lance/span.h"
#include "lance/string.h"

typedef enum {
	LNC_TOKEN_END_OF_FILE,
	LNC_TOKEN_ERROR,

	// Literals
	LNC_TOKEN_IDENTIFIER, // e.g. abc
	LNC_TOKEN_INTEGER,	  // e.g. 42
	LNC_TOKEN_FLOAT,	  // e.g. 4.2
	LNC_TOKEN_BOOLEAN,	  // e.g. true
	LNC_TOKEN_STRING,	  // e.g. "a string"

	// Keywords
	LNC_TOKEN_IF,	 // if
	LNC_TOKEN_ELSE,	 // else
	LNC_TOKEN_THEN,	 // then
	LNC_TOKEN_LET,	 // let
	LNC_TOKEN_IN,	 // in
	LNC_TOKEN_TRUE,	 // true
	LNC_TOKEN_FALSE, // false
	LNC_TOKEN_I8,	 // i8
	LNC_TOKEN_I16,	 // i16
	LNC_TOKEN_I32,	 // i32
	LNC_TOKEN_I64,	 // i64
	LNC_TOKEN_U8,	 // u8
	LNC_TOKEN_U16,	 // u16
	LNC_TOKEN_U32,	 // u32
	LNC_TOKEN_U64,	 // u64
	LNC_TOKEN_F32,	 // f32
	LNC_TOKEN_F64,	 // f64

	// Ponctuation
	LNC_TOKEN_COLON_COLON,		 // ::
	LNC_TOKEN_ARROW,			 // ->
	LNC_TOKEN_FAT_ARROW,		 // =>
	LNC_TOKEN_BIND,				 // =
	LNC_TOKEN_LEFT_PARENTHESIS,	 // (
	LNC_TOKEN_RIGHT_PARENTHESIS, // )
	LNC_TOKEN_LEFT_BRACE,		 // {
	LNC_TOKEN_RIGHT_BRACE,		 // }
	LNC_TOKEN_DOT_LEFT_BRACE,	 // .{
	LNC_TOKEN_DOT,				 // .
	LNC_TOKEN_COMMA,			 // ,
	LNC_TOKEN_SEMICOLON,		 // ;
} LNCTokenKind;

typedef struct {
	LNCTokenKind kind;
	LNCSpan		 span;
	LNCString	 lexeme;
	LNCString	 errorMessage;
} LNCToken;

void lncFreeToken(LNCToken* token);
