#include "parser.h"
#include "diag.h"
#include "string_pool.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static const char *CopyTokenString(const Parser *parser, const Token token) {
	const uint32_t length = token.end - token.begin;
	return InternString(&parser->lexer->source[token.begin], length);
}

static const char *CopyStringToken(const Parser *parser, const Token token) {
	const uint32_t length = token.end - token.begin;
	if (length < 2)
		return InternString("", 0);
	return InternString(&parser->lexer->source[token.begin + 1], length - 2);
}

[[gnu::format(printf, 4, 5)]]
static void ErrorAt(Parser *parser, const Token token, const char *category, const char *format, ...) {
	if (parser->panicMode)
		return;
	parser->panicMode = true;
	parser->hadError  = true;

	char	message[512];
	va_list args = {};
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	ReportError(category, SOURCE_LOC(parser->fileName, token.line, token.column), "%s", message);
}

static void Advance(Parser *parser) {
	parser->current = parser->peek;

	for (;;) {
		parser->peek = LexToken(parser->lexer);

		if (parser->peek.type != TOKEN_ERROR) {
			break;
		}

		ErrorAt(parser, parser->peek, "Lexical Error", "%s", parser->peek.message);
	}
}

void InitializeParser(Parser *parser, Lexer *lexer, const char *fileName, Arena *arena) {
	parser->lexer	  = lexer;
	parser->arena	  = arena;
	parser->fileName  = fileName;
	parser->hadError  = false;
	parser->panicMode = false;

	// Populate parser.current and parser.peek
	Advance(parser);
	Advance(parser);
}

static bool Check(const Parser *parser, const TokenType type) { return parser->current.type == type; }

static bool CheckPeek(const Parser *parser, const TokenType type) { return parser->peek.type == type; }

static bool Match(Parser *parser, const TokenType type) {
	if (!Check(parser, type)) {
		return false;
	}

	Advance(parser);
	return true;
}

static bool Consume(Parser *parser, const TokenType type, const char *message) {
	if (Check(parser, type)) {
		Advance(parser);
		return true;
	}

	ErrorAt(parser, parser->current, "Syntax Error", "%s (got '%s')", message, TokenTypeToString(parser->current.type));
	return false;
}

static AstType *ParseType(Parser *parser);

static bool IsOperatorToken(const TokenType type) { return type == TOKEN_OPERATOR_SYMBOL; }

static const char *ParseFieldName(Parser *parser) {
	if (Check(parser, TOKEN_IDENT)) {
		const char *fieldName = CopyTokenString(parser, parser->current);
		Advance(parser);
		return fieldName;
	}

	if (Match(parser, TOKEN_LPAREN)) {
		if (Check(parser, TOKEN_RPAREN) || Check(parser, TOKEN_EOF)) {
			Consume(parser, TOKEN_IDENT, "Expected operator in parenthesized field name");
			return nullptr;
		}

		const char *fieldName = CopyTokenString(parser, parser->current);
		Advance(parser);
		Consume(parser, TOKEN_RPAREN, "Expected ')' after operator field name");
		return fieldName;
	}

	Consume(parser, TOKEN_IDENT, "Expected field name");
	return nullptr;
}

static AstType *ParsePrimitiveType(Parser *parser) {
	const uint32_t line	  = parser->current.line;
	const uint32_t column = parser->current.column;

	// lazy T: binds tighter than `->`, so `lazy T -> T` is `(lazy T) -> T`
	if (Match(parser, TOKEN_LAZY)) {
		AstType *inner = ParsePrimitiveType(parser);
		return inner ? CreateLazyTypeAst(parser->arena, inner, line, column) : nullptr;
	}

	// inline T: marks the final return type of a function, like `lazy` binds tighter than `->`
	if (Match(parser, TOKEN_INLINE)) {
		AstType *inner = ParsePrimitiveType(parser);
		return inner ? CreateInlineTypeAst(parser->arena, inner, line, column) : nullptr;
	}

	// Primitive keyword or identifier name
	if (parser->current.type > TOKEN_KEYWORD_IMPL_MINIMUM && parser->current.type < TOKEN_KEYWORD_IMPL_MAXIMUM) {
		const char *identifier = CopyTokenString(parser, parser->current);
		Advance(parser);

		return CreateNamedTypeAst(parser->arena, identifier, line, column);
	}

	if (Check(parser, TOKEN_IDENT)) {
		const char *identifier = CopyTokenString(parser, parser->current);
		Advance(parser);

		return CreateNamedTypeAst(parser->arena, identifier, line, column);
	}

	// Anonymous struct types : { field :: Type, ... }
	if (Match(parser, TOKEN_LBRACE)) {
		size_t capacity = 4;
		size_t count	= 0;

		AstFieldDecl *fields = ARENA_ARRAY(parser->arena, AstFieldDecl, capacity);

		while (!Check(parser, TOKEN_RBRACE) && !Check(parser, TOKEN_EOF)) {
			const char *fieldName = ParseFieldName(parser);
			if (!fieldName) {
				break;
			}

			Consume(parser, TOKEN_COLON_COLON, "Expected '::' after field name");
			AstType *fieldType = ParseType(parser);

			if (count >= capacity) {
				capacity *= 2;
				fields = GrowAstArray(parser->arena, fields, count, capacity, sizeof(AstFieldDecl));
			}

			fields[count++] = (AstFieldDecl) {.name = fieldName, .type = fieldType};

			if (!Match(parser, TOKEN_COMMA)) {
				break;
			}
		}

		Consume(parser, TOKEN_RBRACE, "Expected '}' after struct fields");

		return CreateStructTypeAst(parser->arena, fields, count, line, column);
	}

	// Parenthesized type: ( T )
	if (Match(parser, TOKEN_LPAREN)) {
		if (Match(parser, TOKEN_RPAREN)) {
			return CreateNamedTypeAst(parser->arena, "()", line, column);
		}

		// Check if this begins an interface constraint list: (Ident Ident, ...)
		if (Check(parser, TOKEN_IDENT) && CheckPeek(parser, TOKEN_IDENT)) {
			size_t		   capacity	   = 4;
			size_t		   count	   = 0;
			AstConstraint *constraints = ARENA_ARRAY(parser->arena, AstConstraint, capacity);

			while (Check(parser, TOKEN_IDENT) && CheckPeek(parser, TOKEN_IDENT)) {
				const char *ifaceName = CopyTokenString(parser, parser->current);
				Advance(parser);
				const char *paramName = CopyTokenString(parser, parser->current);
				Advance(parser);

				if (count >= capacity) {
					capacity *= 2;
					constraints = GrowAstArray(parser->arena, constraints, count, capacity, sizeof(AstConstraint));
				}

				constraints[count++] = (AstConstraint) {.interfaceName = ifaceName, .typeParam = paramName};

				if (!Match(parser, TOKEN_COMMA)) {
					break;
				}
			}

			Consume(parser, TOKEN_RPAREN, "Expected ')' after interface constraints");

			return CreateConstrainedTypeAst(parser->arena, constraints, count, nullptr, line, column);
		}

		AstType *inner = ParseType(parser);
		Consume(parser, TOKEN_RPAREN, "Expected ')' after parenthesized type");
		return inner;
	}

	ErrorAt(parser, parser->current, "Syntax Error", "Expected type, got '%s'",
			TokenTypeToString(parser->current.type));
	return nullptr;
}

// Parses chained function types: T1 -> T2 -> T3
static AstType *ParseType(Parser *parser) {
	const uint32_t line	  = parser->current.line;
	const uint32_t column = parser->current.column;

	AstType *left = ParsePrimitiveType(parser);

	if (!left) {
		return nullptr;
	}

	// (Numeric T) => TargetType
	if (left->kind == AST_TYPE_CONSTRAINED && Match(parser, TOKEN_FAT_ARROW)) {
		left->constrained.targetType = ParseType(parser);
		return left;
	}

	// T1 -> T2
	if (Match(parser, TOKEN_ARROW)) {
		AstType *right = ParseType(parser);
		return CreateFunctionTypeAst(parser->arena, left, right, line, column);
	}

	return left;
}

static AstExpr *ParseExpr(Parser *parser);

static bool IsTypeKeywordToken(const TokenType type) {
	return type > TOKEN_KEYWORD_IMPL_MINIMUM && type < TOKEN_KEYWORD_IMPL_MAXIMUM;
}

typedef struct {
	const char *name;
	AstExpr	   *value;
	uint32_t	line;
	uint32_t	column;
} LetBinding;

//   let a = x in body
//   let a = x, b = y in body
//   let a = x
//       b = y
//   in body
static AstExpr *ParseLet(Parser *parser, const uint32_t line, const uint32_t column) {
	size_t		capacity = 4;
	size_t		count	 = 0;
	LetBinding *bindings = ARENA_ARRAY(parser->arena, LetBinding, capacity);

	do {
		if (!Check(parser, TOKEN_IDENT)) {
			Consume(parser, TOKEN_IDENT, "Expected name after 'let'");
			return nullptr;
		}
		const Token nameToken = parser->current;
		Advance(parser);
		if (!Consume(parser, TOKEN_EQUAL, "Expected '=' after let name"))
			return nullptr;

		AstExpr *value = ParseExpr(parser);
		if (!value)
			return nullptr;

		if (count >= capacity) {
			capacity *= 2;
			bindings = GrowAstArray(parser->arena, bindings, count, capacity, sizeof(LetBinding));
		}
		bindings[count++] = (LetBinding) {
				.name	= CopyTokenString(parser, nameToken),
				.value	= value,
				.line	= nameToken.line,
				.column = nameToken.column,
		};
	} while (Match(parser, TOKEN_COMMA) || (Check(parser, TOKEN_IDENT) && CheckPeek(parser, TOKEN_EQUAL)));

	if (!Consume(parser, TOKEN_IN, "Expected 'in' after let value"))
		return nullptr;
	AstExpr *body = ParseExpr(parser);
	if (!body)
		return nullptr;

	// `let a = x, b = y in body` is `let a = x in let b = y in body`.
	for (size_t i = count; i > 1; i--) {
		const LetBinding *binding = &bindings[i - 1];
		body = CreateLetExpr(parser->arena, binding->name, binding->value, body, binding->line, binding->column);
	}
	return CreateLetExpr(parser->arena, bindings[0].name, bindings[0].value, body, line, column);
}

static AstExpr *ParsePrimaryExpr(Parser *parser) {
	const uint32_t line	  = parser->current.line;
	const uint32_t column = parser->current.column;

	AstExpr *expr;

	if (Match(parser, TOKEN_LET)) {
		return ParseLet(parser, line, column);
	}

	// Integer literal
	if (Check(parser, TOKEN_INT_LIT)) {
		const int64_t val = strtoll(&parser->lexer->source[parser->current.begin], nullptr, 10);
		Advance(parser);
		expr = CreateIntLitExpr(parser->arena, val, line, column);
		// Float literal
	} else if (Check(parser, TOKEN_FLOAT_LIT)) {
		const double val = strtod(&parser->lexer->source[parser->current.begin], nullptr);
		Advance(parser);
		expr = CreateFloatLitExpr(parser->arena, val, line, column);
		// String literal
	} else if (Check(parser, TOKEN_STRING_LIT)) {
		const char *str = CopyStringToken(parser, parser->current);
		Advance(parser);
		expr = CreateStringLitExpr(parser->arena, str, line, column);
		// Boolean literal
	} else if (Check(parser, TOKEN_BOOL_LIT)) {
		const bool val = (parser->current.end - parser->current.begin == 4); // "true" vs "false"
		Advance(parser);
		expr = CreateBoolLitExpr(parser->arena, val, line, column);
		// Identifier or Operator
	} else if (Check(parser, TOKEN_IDENT) || IsOperatorToken(parser->current.type) ||
			   IsTypeKeywordToken(parser->current.type)) {
		const char *identifier = CopyTokenString(parser, parser->current);
		Advance(parser);
		expr = CreateIdentExpr(parser->arena, identifier, line, column);
		// Anonymous struct type
	} else if (Check(parser, TOKEN_LBRACE)) {
		AstType *type = ParseType(parser);
		expr		  = CreateTypeExpr(parser->arena, type, line, column);
		// Struct literal
	} else if (Match(parser, TOKEN_DOT_LBRACE)) {
		size_t capacity = 4;
		size_t count	= 0;

		AstFieldValue *fields = ARENA_ARRAY(parser->arena, AstFieldValue, capacity);

		while (!Check(parser, TOKEN_RBRACE) && !Check(parser, TOKEN_EOF)) {
			const char *fieldName = ParseFieldName(parser);
			if (!fieldName) {
				break;
			}

			Consume(parser, TOKEN_EQUAL, "Expected '=' after field name in struct value");
			AstExpr *val = ParseExpr(parser);

			if (count >= capacity) {
				capacity *= 2;
				fields = GrowAstArray(parser->arena, fields, count, capacity, sizeof(AstFieldValue));
			}

			fields[count++] = (AstFieldValue) {.name = fieldName, .value = val};

			if (!Match(parser, TOKEN_COMMA)) {
				break;
			}
		}

		Consume(parser, TOKEN_RBRACE, "Expected '}' after struct value");

		expr = CreateStructValueExpr(parser->arena, fields, count, line, column);
		// Comptime expression
	} else if (Match(parser, TOKEN_BACKTICK)) {
		AstExpr *inner = ParsePrimaryExpr(parser);
		expr		   = CreateComptimeExpr(parser->arena, inner, line, column);
		// Parenthesized sub-expression
	} else if (Match(parser, TOKEN_LPAREN)) {
		expr = ParseExpr(parser);
		Consume(parser, TOKEN_RPAREN, "Expected ')' after expression");
	} else {
		ErrorAt(parser, parser->current, "Syntax Error", "Expected expression, got '%s'",
				TokenTypeToString(parser->current.type));
		return nullptr;
	}

	// Postfix field access
	while (Match(parser, TOKEN_DOT)) {
		const char *fieldName = ParseFieldName(parser);
		if (!fieldName) {
			break;
		}

		expr = CreateFieldAccessExpr(parser->arena, expr, fieldName, line, column);
	}

	return expr;
}

static bool CanStartExpr(const TokenType type) {
	return type == TOKEN_INT_LIT || type == TOKEN_FLOAT_LIT || type == TOKEN_STRING_LIT || type == TOKEN_BOOL_LIT ||
		   type == TOKEN_IDENT || IsOperatorToken(type) || type == TOKEN_DOT_LBRACE || type == TOKEN_BACKTICK ||
		   type == TOKEN_LPAREN || type == TOKEN_LBRACE || type == TOKEN_LET || IsTypeKeywordToken(type);
}

static bool CanTakeArguments(const AstExpr *expr) {
	if (!expr) {
		return false;
	}

	if (expr->kind == AST_EXPR_TYPE || expr->kind == AST_EXPR_STRUCT_VALUE || expr->kind == AST_EXPR_LET) {
		return false;
	}

	return true;
}

static bool CanConsumeArgument(const Parser *parser, const AstExpr *callee) {
	if (!CanTakeArguments(callee)) {
		return false;
	}

	if (!CanStartExpr(parser->current.type)) {
		return false;
	}

	// `name ::` starts a declaration, `name =` the next let binding.
	if (parser->current.type == TOKEN_IDENT &&
		(parser->peek.type == TOKEN_COLON_COLON || parser->peek.type == TOKEN_EQUAL)) {
		return false;
	}

	// A token in the first column of a new line starts the next declaration
	if (parser->current.line > callee->line && parser->current.column == 0) {
		return false;
	}

	return true;
}

static bool CanBeParam(const TokenType type) { return type == TOKEN_IDENT || IsTypeKeywordToken(type); }

static AstExpr *ParseExpr(Parser *parser) {
	AstExpr *callee = ParsePrimaryExpr(parser);

	if (!callee) {
		return nullptr;
	}

	while (CanConsumeArgument(parser, callee)) {
		AstExpr *arg = ParsePrimaryExpr(parser);

		if (!arg) {
			break;
		}

		callee = CreateCallExpr(parser->arena, callee, arg, callee->line, callee->column);
	}

	return callee;
}

static AstDecl ParseImportDeclaration(Parser *parser) {
	AstDecl decl = {0};
	decl.kind	 = AST_DECL_IMPORT;
	decl.file	 = parser->fileName;
	decl.line	 = parser->current.line;
	decl.column	 = parser->current.column;
	Advance(parser);

	if (!Check(parser, TOKEN_STRING_LIT)) {
		Consume(parser, TOKEN_STRING_LIT, "Expected module path string after 'import'");
		return decl;
	}

	decl.modulePath = CopyStringToken(parser, parser->current);
	Advance(parser);
	return decl;
}

static AstDecl ParseDeclaration(Parser *parser) {
	AstDecl decl = {0};

	if (!Check(parser, TOKEN_IDENT) && !Check(parser, TOKEN_LPAREN)) {
		Consume(parser, TOKEN_IDENT, "Expected declaration name");
		return decl;
	}

	decl.file	= parser->fileName;
	decl.line	= parser->current.line;
	decl.column = parser->current.column;
	decl.name	= ParseFieldName(parser);

	size_t		 capacity	= 4;
	size_t		 paramCount = 0;
	const char **params		= ARENA_ARRAY(parser->arena, const char *, capacity);

	while (CanBeParam(parser->current.type)) {
		if (paramCount >= capacity) {
			capacity *= 2;
			params = GrowAstArray(parser->arena, params, paramCount, capacity, sizeof(const char *));
		}

		params[paramCount++] = CopyTokenString(parser, parser->current);
		Advance(parser);
	}

	decl.params		= params;
	decl.paramCount = paramCount;

	// Type Annotation
	if (Match(parser, TOKEN_COLON_COLON)) {
		decl.kind			= AST_DECL_TYPE_ANNOTATION;
		decl.typeAnnotation = ParseType(parser);

		return decl;
	}

	// Value / Function Binding
	decl.kind = AST_DECL_BINDING;
	Consume(parser, TOKEN_EQUAL, "Expected '=' in binding");

	decl.body = ParseExpr(parser);

	return decl;
}

// Skips tokens until the start of the next top-level declaration (a token in
// the first column of a later line), so one syntax error doesn't hide others.
static void Synchronize(Parser *parser, const uint32_t declarationLine) {
	parser->panicMode = false;
	while (!Check(parser, TOKEN_EOF) && !(parser->current.column == 0 && parser->current.line > declarationLine)) {
		Advance(parser);
	}
}

AstModule *ParseModule(Parser *parser) {
	AstModule *module	 = ARENA_NEW(parser->arena, AstModule);
	size_t	   capacity	 = 8;
	module->file		 = parser->fileName;
	module->declarations = ARENA_ARRAY(parser->arena, AstDecl, capacity);

	while (!Check(parser, TOKEN_EOF)) {
		if (module->count >= capacity) {
			capacity *= 2;
			module->declarations =
					GrowAstArray(parser->arena, module->declarations, module->count, capacity, sizeof(AstDecl));
		}

		const uint32_t declarationLine = parser->current.line;
		if (Check(parser, TOKEN_IMPORT)) {
			module->declarations[module->count++] = ParseImportDeclaration(parser);
		} else {
			module->declarations[module->count++] = ParseDeclaration(parser);
		}

		if (parser->panicMode) {
			Synchronize(parser, declarationLine);
		}
	}

	return module;
}