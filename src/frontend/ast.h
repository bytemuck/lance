//
// Created by amelia on 9/27/26.
//

#ifndef LANCE_AST_H
#define LANCE_AST_H

#include <stdint.h>
#include <stdlib.h>

#include "token.h"

typedef struct {
	const char* interfaceName;
	const char* typeParam;
} AstConstraint;

typedef enum {
    AST_TYPE_NAMED,       // e.g. 'i32', 'Vec2'
    AST_TYPE_FUNCTION,    // e.g. 'T1 -> T2'
    AST_TYPE_STRUCT,      // e.g. '{ x :: T, y :: T }'
    AST_TYPE_CONSTRAINED, // e.g. (Interface T) => ReturnType
} AstTypeKind;

typedef struct AstType AstType;

typedef struct {
    const char* name;
    AstType* type;
} AstFieldDecl;

struct AstType {
    AstTypeKind kind;
    uint32_t line;
    uint32_t column;

    union {
        // AST_TYPE_NAMED
        struct {
            const char* name;
        } named;

        // AST_TYPE_FUNCTION (curried: paramType -> returnType)
        struct {
            AstType* paramType;
            AstType* returnType;
        } function;

        // AST_TYPE_STRUCT
        struct {
            AstFieldDecl* fields;
            size_t fieldCount;
        } structType;

        // AST_TYPE_CONSTRAINED
        struct {
        	AstConstraint* constraints;
        	size_t constraintCount;
            AstType* targetType;
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
    AST_EXPR_FIELD_ACCESS,
    AST_EXPR_STRUCT_VALUE,
    AST_EXPR_COMPTIME,
    AST_EXPR_TYPE,
} AstExprKind;

typedef struct AstExpr AstExpr;

typedef struct {
    const char* name;
    AstExpr* value;
} AstFieldValue;

struct AstExpr {
    AstExprKind kind;
    uint32_t line;
    uint32_t column;

    union {
        int64_t intVal;
        double floatVal;
        const char* stringVal;
        bool boolVal;
        const char* identName;
        AstType* typeExpr;

        // Function call: callee arg
        struct {
            AstExpr* callee;
            AstExpr* argument;
        } call;

        // Field access: target.field
        struct {
            AstExpr* target;
            const char* fieldName;
        } fieldAccess;

        // Struct value: .{ x = 1.0, y = 2.0 }
        struct {
            AstFieldValue* fields;
            size_t fieldCount;
        } structValue;

        // Compile-time: `expr
        struct {
            AstExpr* inner;
        } comptime;
    };
};

typedef enum {
    AST_DECL_IMPORT,          // import "module.lance"
    AST_DECL_TYPE_ANNOTATION, // name :: Type
    AST_DECL_BINDING,         // name arg1 ... = body
} AstDeclKind;

typedef struct {
    AstDeclKind kind;
    const char* name;
    const char** params;
    size_t paramCount;
    const char* file; // Source file the declaration was parsed from
    uint32_t line;
    uint32_t column;

    union {
        const char* modulePath;
        AstType* typeAnnotation;
        AstExpr* body;
    };
} AstDecl;

typedef struct {
    AstDecl* declarations;
    size_t count;
} AstModule;

AstType* CreateNamedTypeAst(const char* name, uint32_t line, uint32_t column);
AstType* CreateFunctionTypeAst(AstType* paramType, AstType* returnType, uint32_t line, uint32_t column);
AstType* CreateStructTypeAst(AstFieldDecl* fields, size_t fieldCount, uint32_t line, uint32_t column);
AstType* CreateConstrainedTypeAst(AstConstraint* constraints, size_t count, AstType* targetType, uint32_t line, uint32_t column);
AstType* CloneAstType(const AstType* type);
void FreeTypeAst(AstType* type);

AstExpr* CreateIntLitExpr(int64_t value, uint32_t line, uint32_t column);
AstExpr* CreateFloatLitExpr(double value, uint32_t line, uint32_t column);
AstExpr* CreateStringLitExpr(const char* value, uint32_t line, uint32_t column);
AstExpr* CreateBoolLitExpr(bool value, uint32_t line, uint32_t column);
AstExpr* CreateIdentExpr(const char* name, uint32_t line, uint32_t column);
AstExpr* CreateTypeExpr(AstType* type, uint32_t line, uint32_t column);
AstExpr* CreateCallExpr(AstExpr* callee, AstExpr* args, uint32_t line, uint32_t column);
AstExpr* CreateFieldAccessExpr(AstExpr* base, const char* fieldName, uint32_t line, uint32_t column);
AstExpr* CreateStructValueExpr(AstFieldValue* fields, size_t fieldCount, uint32_t line, uint32_t column);
AstExpr* CreateComptimeExpr(AstExpr* expr, uint32_t line, uint32_t column);
void FreeExprAst(AstExpr* expr);

void FreeDeclAst(AstDecl* decl);
void FreeModuleAst(AstModule* module);

#endif //LANCE_AST_H
