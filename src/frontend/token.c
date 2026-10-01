//
// Created by amelia on 9/27/26.
//

#include "token.h"

const char *TokenTypeToString(const TokenType type) {
	switch (type) {
		case TOKEN_EOF:
			return "TOKEN_EOF";
		case TOKEN_ERROR:
			return "TOKEN_ERROR";
		case TOKEN_IDENT:
			return "TOKEN_IDENT";
		case TOKEN_INT_LIT:
			return "TOKEN_INT_LIT";
		case TOKEN_FLOAT_LIT:
			return "TOKEN_FLOAT_LIT";
		case TOKEN_STRING_LIT:
			return "TOKEN_STRING_LIT";
		case TOKEN_BOOL_LIT:
			return "TOKEN_BOOL_LIT";

		// Keywords
		case TOKEN_KEYWORD_TYPE:
			return "TOKEN_KEYWORD_TYPE";
		case TOKEN_KEYWORD_BOOL:
			return "TOKEN_KEYWORD_BOOL";
		case TOKEN_KEYWORD_STRING:
			return "TOKEN_KEYWORD_STRING";
		case TOKEN_KEYWORD_I8:
			return "TOKEN_KEYWORD_I8";
		case TOKEN_KEYWORD_I16:
			return "TOKEN_KEYWORD_I16";
		case TOKEN_KEYWORD_I32:
			return "TOKEN_KEYWORD_I32";
		case TOKEN_KEYWORD_I64:
			return "TOKEN_KEYWORD_I64";
		case TOKEN_KEYWORD_U8:
			return "TOKEN_KEYWORD_U8";
		case TOKEN_KEYWORD_U16:
			return "TOKEN_KEYWORD_U16";
		case TOKEN_KEYWORD_U32:
			return "TOKEN_KEYWORD_U32";
		case TOKEN_KEYWORD_U64:
			return "TOKEN_KEYWORD_U64";
		case TOKEN_KEYWORD_F32:
			return "TOKEN_KEYWORD_F32";
		case TOKEN_KEYWORD_F64:
			return "TOKEN_KEYWORD_F64";
		case TOKEN_IMPORT:
			return "'import'";
		case TOKEN_LET:
			return "'let'";
		case TOKEN_IN:
			return "'in'";
		case TOKEN_LAZY:
			return "'lazy'";
		case TOKEN_INLINE:
			return "'inline'";

		// Syntax & Punctuation
		case TOKEN_COLON_COLON:
			return "TOKEN_COLON_COLON";
		case TOKEN_ARROW:
			return "TOKEN_ARROW";
		case TOKEN_FAT_ARROW:
			return "TOKEN_FAT_ARROW";
		case TOKEN_EQUAL:
			return "TOKEN_EQUAL";
		case TOKEN_DOT_LBRACE:
			return "TOKEN_DOT_LBRACE";
		case TOKEN_DOT:
			return "TOKEN_DOT";
		case TOKEN_LBRACE:
			return "TOKEN_LBRACE";
		case TOKEN_RBRACE:
			return "TOKEN_RBRACE";
		case TOKEN_LPAREN:
			return "TOKEN_LPAREN";
		case TOKEN_RPAREN:
			return "TOKEN_RPAREN";
		case TOKEN_COMMA:
			return "TOKEN_COMMA";
		case TOKEN_BACKTICK:
			return "TOKEN_BACKTICK";

		case TOKEN_OPERATOR_SYMBOL:
			return "TOKEN_OPERATOR_SYMBOL";

		default:
			return "UNKNOWN";
	}
}
