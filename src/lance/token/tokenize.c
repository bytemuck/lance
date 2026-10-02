#include "lance/token/tokenize.h"

#include <stdbool.h>
#include <string.h>

#include <stdio.h>

LNCTokenizer lncMakeTokenizer(LNCStringView source) {
	return (LNCTokenizer) {
			.source = source,
			.span	= lncMakeSpan(0, 0),
			.cursor = lncMakeCursor(1, 1),
	};
}

LNCToken lncMakeErrorToken(LNCTokenizer* tokenizer, LNCTokenKind kind, const char* errorMessage) {
	const LNCToken token = (LNCToken) {
			.kind		  = kind,
			.span		  = tokenizer->span,
			.lexeme		  = lncCopyStringView(lncSubstringView(tokenizer->source, tokenizer->span)),
			.errorMessage = lncMakeString(errorMessage),
	};

	tokenizer->span = lncEndSpan(tokenizer->span);

	return token;
}

LNCToken lncMakeToken(LNCTokenizer* tokenizer, LNCTokenKind kind) {
	return lncMakeErrorToken(tokenizer, kind, NULL);
}


char lncAdvance(LNCTokenizer* tokenizer) {
	const char c = tokenizer->source.items[tokenizer->span.end];

	if (c == '\0') {
		return c;
	}

	tokenizer->span	  = lncAdvanceSpan(tokenizer->span);
	tokenizer->cursor = lncAdvanceCursor(tokenizer->cursor);

	if (c == '\n') {
		tokenizer->cursor = lncEndCursor(tokenizer->cursor);
	}

	return c;
}

char lncPeek(LNCTokenizer* tokenizer) {
	return tokenizer->source.items[tokenizer->span.end];
}

char lncPeekN(LNCTokenizer* tokenizer, uint32_t n) {
	return tokenizer->source.items[tokenizer->span.end + n];
}

bool lncNoise(LNCTokenizer* tokenizer) {
	const char p = lncPeek(tokenizer);

	return p == ' ' || p == '\t' || p == '\r' || p == '\n';
}

bool lncNoiseN(LNCTokenizer* tokenizer, uint32_t n) {
	const char p = lncPeekN(tokenizer, n);

	return p == ' ' || p == '\t' || p == '\r' || p == '\n';
}

bool lncNoiseBeginN(LNCTokenizer* tokenizer, uint32_t n) {
	const char p = tokenizer->source.items[tokenizer->span.begin + n];

	return p == ' ' || p == '\t' || p == '\r' || p == '\n';
};

bool lncMatch(LNCTokenizer* tokenizer, const char* string) {
	const uint32_t length  = strlen(string);
	const char*	   current = tokenizer->source.items + tokenizer->span.begin;

	if (strncmp(current, string, length) != 0) {
		return false;
	}

	tokenizer->span	  = lncAdvanceSpanN(tokenizer->span, length - 1);
	tokenizer->cursor = lncAdvanceCursorN(tokenizer->cursor, length - 1);

	return true;
}

void lncSkipNoise(LNCTokenizer* tokenizer) {
	while (lncNoise(tokenizer)) {
		lncAdvance(tokenizer);
	}

	tokenizer->span = lncEndSpan(tokenizer->span);
}

static const struct {
	const char*	 string;
	LNCTokenKind type;
} kPunctuations[] = {
		{.string = "::", .type = LNC_TOKEN_COLON_COLON},
		{.string = "->", .type = LNC_TOKEN_ARROW},
		{.string = "=>", .type = LNC_TOKEN_FAT_ARROW},
		{.string = "=", .type = LNC_TOKEN_BIND},
		{.string = "(", .type = LNC_TOKEN_LEFT_PARENTHESIS},
		{.string = ")", .type = LNC_TOKEN_RIGHT_PARENTHESIS},
		{.string = "{", .type = LNC_TOKEN_LEFT_BRACE},
		{.string = "}", .type = LNC_TOKEN_RIGHT_BRACE},
		{.string = ".{", .type = LNC_TOKEN_DOT_LEFT_BRACE},
		{.string = ".", .type = LNC_TOKEN_DOT},
		{.string = ",", .type = LNC_TOKEN_COMMA},
		{.string = ";", .type = LNC_TOKEN_SEMICOLON},
};


static const struct {
	const char*	 string;
	LNCTokenKind type;
} kKeywords[] = {{.string = "if", .type = LNC_TOKEN_IF},	   {.string = "else", .type = LNC_TOKEN_ELSE},
				 {.string = "then", .type = LNC_TOKEN_THEN},   {.string = "let", .type = LNC_TOKEN_LET},
				 {.string = "in", .type = LNC_TOKEN_IN},	   {.string = "true", .type = LNC_TOKEN_TRUE},
				 {.string = "false", .type = LNC_TOKEN_FALSE}, {.string = "i8", .type = LNC_TOKEN_I8},
				 {.string = "i16", .type = LNC_TOKEN_I16},	   {.string = "i32", .type = LNC_TOKEN_I32},
				 {.string = "i64", .type = LNC_TOKEN_I64},	   {.string = "u8", .type = LNC_TOKEN_U8},
				 {.string = "u16", .type = LNC_TOKEN_U16},	   {.string = "u32", .type = LNC_TOKEN_U32},
				 {.string = "u64", .type = LNC_TOKEN_U64},	   {.string = "f32", .type = LNC_TOKEN_F32},
				 {.string = "f64", .type = LNC_TOKEN_F64}};

bool lncAlpha(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool lncNumeric(char c) {
	return c >= '0' && c <= '9';
}

bool lncAlphaNumeric(char c) {
	return lncAlpha(c) || lncNumeric(c);
}

LNCToken lncString(LNCTokenizer* tokenizer) {
	while (lncPeek(tokenizer) != '"') {
		if (tokenizer->source.items[tokenizer->span.end] == '\0') {
			return lncMakeErrorToken(tokenizer, LNC_TOKEN_ERROR, "Unterminated string literal.");
		}

		lncAdvance(tokenizer);
	}

	return lncMakeToken(tokenizer, LNC_TOKEN_STRING);
}

LNCToken lncNumber(LNCTokenizer* tokenizer) {
	while (lncNumeric(lncPeek(tokenizer))) {
		lncAdvance(tokenizer);
	}

	if (lncPeek(tokenizer) == '.') {
		lncAdvance(tokenizer);

		while (lncAlphaNumeric(lncPeek(tokenizer))) {
			lncAdvance(tokenizer);
		}

		return lncMakeToken(tokenizer, LNC_TOKEN_FLOAT);
	}

	return lncMakeToken(tokenizer, LNC_TOKEN_INTEGER);
}

LNCToken lncIdentifier(LNCTokenizer* tokenizer) {
	while (lncAlphaNumeric(lncPeek(tokenizer))) {
		lncAdvance(tokenizer);
	}

	const uint32_t length = tokenizer->span.end - tokenizer->span.begin;
	const char*	   lexeme = tokenizer->source.items + tokenizer->span.begin;

	for (size_t i = 0; i < sizeof(kKeywords) / sizeof(kKeywords[0]); ++i) {
		const char* keyword = kKeywords[i].string;

		if (strlen(keyword) == length && strncmp(lexeme, keyword, length) == 0) {
			return lncMakeToken(tokenizer, kKeywords[i].type);
		}
	}

	return lncMakeToken(tokenizer, LNC_TOKEN_IDENTIFIER);
}

LNCToken lncTokenize(LNCTokenizer* tokenizer) {
	lncSkipNoise(tokenizer);

	const char c = lncAdvance(tokenizer);

	if (c == '\0') {
		return lncMakeToken(tokenizer, LNC_TOKEN_END_OF_FILE);
	}

	// Check for punctuations
	for (size_t i = 0; i < sizeof(kPunctuations) / sizeof(kPunctuations[0]); ++i) {
		if (lncMatch(tokenizer, kPunctuations[i].string)) {
			return lncMakeToken(tokenizer, kPunctuations[i].type);
		}
	}

	if (c == '"') {
		return lncString(tokenizer);
	}

	if (lncAlpha(c)) {
		return lncIdentifier(tokenizer);
	}

	if (lncNumeric(c)) {
		return lncNumber(tokenizer);
	}

	return lncMakeErrorToken(tokenizer, LNC_TOKEN_ERROR, "Unexpected character.");
}
