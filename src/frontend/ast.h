#ifndef LANCE_AST_H
#define LANCE_AST_H

#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "token.h"

typedef struct {
	const char *interfaceName;
	const char *typeParam;
} AstConstraint;

typedef enum {
	AST_TYPE_NAMED,		  // e.g. 'i32', 'Vec2'
	AST_TYPE_FUNCTION,	  // e.g. 'T1 -> T2'
	AST_TYPE_STRUCT,	  // e.g. '{ x :: T, y :: T }'
	AST_TYPE_CONSTRAINED, // e.g. (Interface T) => ReturnType
	AST_TYPE_LAZY,		  // e.g. 'lazy T', only valid as a parameter type
} AstTypeKind;

typedef struct AstType AstType;

typedef struct {
	const char *name;
	AstType	   *type;
} AstFieldDecl;

struct AstType {
	AstTypeKind kind;
	uint32_t	line;
	uint32_t	column;

	union {
		// AST_TYPE_NAMED
		struct {
			const char *name;
		} named;

		// AST_TYPE_FUNCTION
		struct {
			AstType *paramType;
			AstType *returnType;
		} function;

		// AST_TYPE_STRUCT
		struct {
			AstFieldDecl *fields;
			size_t		  fieldCount;
		} structType;

		// AST_TYPE_LAZY
		struct {
			AstType *inner;
		} lazy;

		// AST_TYPE_CONSTRAINED
		struct {
			AstConstraint *constraints;
			size_t		   constraintCount;
			AstType		  *targetType;
		} constrained;
	};
};

typedef enum {
	AST_EXPR_INT_LIT,
	AST_EXPR_FLOAT_LIT,
	AST_EXPR_STRING_LIT,
	AST_EXPR_BOOL_LIT,
	AST_EXPR_IDENT,
	AST_EXPR_CALL,
	AST_EXPR_LET,
	AST_EXPR_FIELD_ACCESS,
	AST_EXPR_STRUCT_VALUE,
	AST_EXPR_COMPTIME,
	AST_EXPR_TYPE,
} AstExprKind;

typedef struct AstExpr AstExpr;

typedef struct {
	const char *name;
	AstExpr	   *value;
} AstFieldValue;

struct AstExpr {
	AstExprKind kind;
	uint32_t	line;
	uint32_t	column;

	union {
		int64_t		intVal;
		double		floatVal;
		const char *stringVal;
		bool		boolVal;
		const char *identName;
		AstType	   *typeExpr;

		// Function call: callee arg
		struct {
			AstExpr *callee;
			AstExpr *argument;
		} call;

		// let name = value in body
		struct {
			const char *name;
			AstExpr	   *value;
			AstExpr	   *body;
		} let;

		// Field access: target.field
		struct {
			AstExpr	   *target;
			const char *fieldName;
		} fieldAccess;

		// Struct value: .{ x = 1.0, y = 2.0 }
		struct {
			AstFieldValue *fields;
			size_t		   fieldCount;
		} structValue;

		// Compile-time: `expr
		struct {
			AstExpr *inner;
		} comptime;
	};
};

typedef enum {
	AST_DECL_IMPORT,		  // import "module.lance"
	AST_DECL_TYPE_ANNOTATION, // name :: Type
	AST_DECL_BINDING,		  // name arg1 ... = body
} AstDeclKind;

typedef struct {
	AstDeclKind	 kind;
	const char	*name;
	const char **params;
	size_t		 paramCount;
	const char	*file;
	uint32_t	 line;
	uint32_t	 column;

	union {
		const char *modulePath;
		AstType	   *typeAnnotation;
		AstExpr	   *body;
	};
} AstDecl;

typedef struct AstModule AstModule;

// `import "util.lance"` resolved to the loaded module, visible as `util`.
typedef struct {
	const char *alias;
	AstModule  *module;
} AstImport;

struct AstModule {
	const char *file; // Name used in diagnostics
	const char *name; // Qualifies the module's globals at runtime; null for the entry module
	AstDecl	   *declarations;
	size_t		count;
	AstImport  *imports;
	size_t		importCount;
};

void *GrowAstArray(Arena *arena, const void *items, size_t oldCount, size_t newCount, size_t itemSize);

AstType *CreateNamedTypeAst(Arena *arena, const char *name, uint32_t line, uint32_t column);
AstType *CreateFunctionTypeAst(Arena *arena, AstType *paramType, AstType *returnType, uint32_t line, uint32_t column);
AstType *CreateStructTypeAst(Arena *arena, AstFieldDecl *fields, size_t fieldCount, uint32_t line, uint32_t column);
AstType *CreateConstrainedTypeAst(Arena *arena, AstConstraint *constraints, size_t count, AstType *targetType,
								  uint32_t line, uint32_t column);
AstType *CreateLazyTypeAst(Arena *arena, AstType *inner, uint32_t line, uint32_t column);
AstType *CloneAstType(Arena *arena, const AstType *type);

AstExpr *CreateIntLitExpr(Arena *arena, int64_t value, uint32_t line, uint32_t column);
AstExpr *CreateFloatLitExpr(Arena *arena, double value, uint32_t line, uint32_t column);
AstExpr *CreateStringLitExpr(Arena *arena, const char *value, uint32_t line, uint32_t column);
AstExpr *CreateBoolLitExpr(Arena *arena, bool value, uint32_t line, uint32_t column);
AstExpr *CreateIdentExpr(Arena *arena, const char *name, uint32_t line, uint32_t column);
AstExpr *CreateTypeExpr(Arena *arena, AstType *type, uint32_t line, uint32_t column);
AstExpr *CreateCallExpr(Arena *arena, AstExpr *callee, AstExpr *args, uint32_t line, uint32_t column);
AstExpr *CreateLetExpr(Arena *arena, const char *name, AstExpr *value, AstExpr *body, uint32_t line, uint32_t column);
AstExpr *CreateFieldAccessExpr(Arena *arena, AstExpr *base, const char *fieldName, uint32_t line, uint32_t column);
AstExpr *CreateStructValueExpr(Arena *arena, AstFieldValue *fields, size_t fieldCount, uint32_t line, uint32_t column);
AstExpr *CreateComptimeExpr(Arena *arena, AstExpr *expr, uint32_t line, uint32_t column);

#endif // LANCE_AST_H
