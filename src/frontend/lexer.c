#include "lexer.h"

#include <stdio.h>
#include <string.h>

void InitializeLexer(Lexer *lexer, const char *source) {
	lexer->source	   = source;
	lexer->begin	   = 0;
	lexer->end		   = 0;
	lexer->line		   = 1;
	lexer->column	   = 0;
	lexer->beginLine   = 1;
	lexer->beginColumn = 0;
}

static void SkipNoise(Lexer *lexer) {
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

static char Peek(const Lexer *lexer) { return lexer->source[lexer->end]; }

static char PeekNext(const Lexer *lexer) { return Peek(lexer) == '\0' ? '\0' : lexer->source[lexer->end + 1]; }

static char Advance(Lexer *lexer) {
	lexer->column++;

	if (lexer->source[lexer->end] == '\n') {
		lexer->line++;
		lexer->column = 0;
	}

	return lexer->source[lexer->end++];
}

static bool IsEOF(const Lexer *lexer) { return lexer->source[lexer->end] == '\0'; }

static bool IsAlpha(const char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

static bool IsNumeric(const char c) { return c >= '0' && c <= '9'; }

static bool IsAlphaNumeric(const char c) { return IsAlpha(c) || IsNumeric(c); }

static bool IsOperatorCharacter(const char c) {
	switch (c) {
		case '!':
		case '#':
		case '$':
		case '%':
		case '&':
		case '*':
		case '+':
		case '-':
		case '/':
		case '<':
		case '>':
		case '?':
		case '^':
		case '|':
		case '~':
		case '\\':
		case '=':
			return true;
		default:
			return false;
	}
}

static bool Match(Lexer *lexer, const char expected) {
	if (IsEOF(lexer) || lexer->source[lexer->end] != expected) {
		return false;
	}

	lexer->end++;
	lexer->column++;

	return true;
}

static bool MatchN(const Lexer *lexer, const char *expected, const uint32_t length) {
	const uint32_t expectedLength = strlen(expected);
	return length == expectedLength && strncmp(lexer->source + lexer->begin, expected, expectedLength) == 0;
}

static Token MakeToken(const Lexer *lexer, const TokenType type) {
	return (Token) {
			.type	 = type,
			.begin	 = lexer->begin,
			.end	 = lexer->end,
			.line	 = lexer->beginLine,
			.column	 = lexer->beginColumn,
			.message = nullptr,
	};
}

static Token ErrorToken(const Lexer *lexer, const char *message) {
	Token token	  = MakeToken(lexer, TOKEN_ERROR);
	token.message = message;
	return token;
}

static const struct {
	const char *word;
	TokenType	type;
} kKeywords[] = {
		{.word = "type", .type = TOKEN_KEYWORD_TYPE},
		{.word = "i8", .type = TOKEN_KEYWORD_I8},
		{.word = "i16", .type = TOKEN_KEYWORD_I16},
		{.word = "i32", .type = TOKEN_KEYWORD_I32},
		{.word = "i64", .type = TOKEN_KEYWORD_I64},
		{.word = "u8", .type = TOKEN_KEYWORD_U8},
		{.word = "u16", .type = TOKEN_KEYWORD_U16},
		{.word = "u32", .type = TOKEN_KEYWORD_U32},
		{.word = "u64", .type = TOKEN_KEYWORD_U64},
		{.word = "f32", .type = TOKEN_KEYWORD_F32},
		{.word = "f64", .type = TOKEN_KEYWORD_F64},
		{.word = "bool", .type = TOKEN_KEYWORD_BOOL},
		{.word = "string", .type = TOKEN_KEYWORD_STRING},
		{.word = "true", .type = TOKEN_BOOL_LIT},
		{.word = "false", .type = TOKEN_BOOL_LIT},
		{.word = "import", .type = TOKEN_IMPORT},
		{.word = "let", .type = TOKEN_LET},
		{.word = "in", .type = TOKEN_IN},
		{.word = "lazy", .type = TOKEN_LAZY},
};

static Token LexIdentifier(Lexer *lexer) {
	for (char c = Peek(lexer); IsAlphaNumeric(c); Advance(lexer), c = Peek(lexer)) {
	}

	// Named primitives end in `#`, like the operator ones: `if#`.
	Match(lexer, '#');

	const uint32_t length = lexer->end - lexer->begin;
	for (size_t i = 0; i < sizeof(kKeywords) / sizeof(kKeywords[0]); i++) {
		if (MatchN(lexer, kKeywords[i].word, length)) {
			return MakeToken(lexer, kKeywords[i].type);
		}
	}

	return MakeToken(lexer, TOKEN_IDENT);
}

static Token LexNumber(Lexer *lexer) {
	for (char c = Peek(lexer); IsNumeric(c); Advance(lexer), c = Peek(lexer)) {
	}

	const char p  = Peek(lexer);
	const char pn = PeekNext(lexer);

	if (p == '.' && IsNumeric(pn)) {
		Advance(lexer);

		for (char c = Peek(lexer); IsNumeric(c); Advance(lexer), c = Peek(lexer)) {
		}
		return MakeToken(lexer, TOKEN_FLOAT_LIT);
	}

	return MakeToken(lexer, TOKEN_INT_LIT);
}

static Token LexString(Lexer *lexer) {
	for (char c = Peek(lexer); c != '"' && !IsEOF(lexer); Advance(lexer), c = Peek(lexer)) {
	}

	if (IsEOF(lexer)) {
		return ErrorToken(lexer, "Unterminated string literal");
	}

	Advance(lexer);
	return MakeToken(lexer, TOKEN_STRING_LIT);
}

Token LexToken(Lexer *lexer) {
	SkipNoise(lexer);

	lexer->begin	   = lexer->end;
	lexer->beginLine   = lexer->line;
	lexer->beginColumn = lexer->column;

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

	if (c == '-' && Match(lexer, '>'))
		return MakeToken(lexer, TOKEN_ARROW);
	if (c == '=' && Match(lexer, '>'))
		return MakeToken(lexer, TOKEN_FAT_ARROW);

	if (c == '=') {
		if (Match(lexer, '=')) {
			while (IsOperatorCharacter(Peek(lexer))) {
				Advance(lexer);
			}
			return MakeToken(lexer, TOKEN_OPERATOR_SYMBOL);
		}
		return MakeToken(lexer, TOKEN_EQUAL);
	}

	if (IsOperatorCharacter(c)) {
		while (IsOperatorCharacter(Peek(lexer))) {
			Advance(lexer);
		}
		return MakeToken(lexer, TOKEN_OPERATOR_SYMBOL);
	}

	switch (c) {
		case '(':
			return MakeToken(lexer, TOKEN_LPAREN);
		case ')':
			return MakeToken(lexer, TOKEN_RPAREN);
		case '{':
			return MakeToken(lexer, TOKEN_LBRACE);
		case '}':
			return MakeToken(lexer, TOKEN_RBRACE);
		case ',':
			return MakeToken(lexer, TOKEN_COMMA);
		case '`':
			return MakeToken(lexer, TOKEN_BACKTICK);

		case '@': {
			while (IsAlphaNumeric(Peek(lexer))) {
				Advance(lexer);
			}

			return MakeToken(lexer, TOKEN_IDENT);
		}

		case ':':
			if (Match(lexer, ':'))
				return MakeToken(lexer, TOKEN_COLON_COLON);
			return ErrorToken(lexer, "Expected '::'");

		case '.':
			if (Match(lexer, '{'))
				return MakeToken(lexer, TOKEN_DOT_LBRACE);
			return MakeToken(lexer, TOKEN_DOT);

		default:
			return ErrorToken(lexer, "Unexpected character");
	}
}