#ifndef LANCE_TYPED_AST_H
#define LANCE_TYPED_AST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "type.h"

typedef enum {
    TYPED_EXPR_INT_LIT,
    TYPED_EXPR_FLOAT_LIT,
    TYPED_EXPR_BOOL_LIT,
    TYPED_EXPR_STRING_LIT,
    TYPED_EXPR_VAR,
    TYPED_EXPR_CALL,
    TYPED_EXPR_FIELD_ACCESS,
    TYPED_EXPR_STRUCT_INIT
} TypedExprKind;

typedef struct TypedExpr TypedExpr;

typedef struct {
    const char* name;
    TypedExpr* value;
} TypedFieldValue;

struct TypedExpr {
    TypedExprKind kind;
    LanceType* type;
    uint32_t line;
    uint32_t column;
    union {
        int64_t intVal;
        double floatVal;
        bool boolVal;
        const char* stringVal;
        const char* varName;

        struct {
            TypedExpr* callee;
            TypedExpr* argument;
        } call;

        struct {
            TypedExpr* target;
            size_t fieldIndex;
            const char* fieldName;
        } fieldAccess;

        struct {
            LanceType* structType;
            TypedFieldValue* fields;
            size_t fieldCount;
        } structInit;
    };
};

typedef struct {
    const char* name;
    LanceType* type;
    const char** params;
    size_t paramCount;
    TypedExpr* body;
    uint32_t line;
    uint32_t column;
} TypedDecl;

typedef struct {
    TypedDecl* declarations;
    size_t count;
    size_t capacity;
} TypedModule;

// Constructors
TypedExpr* CreateTypedIntLitExpr(int64_t val, LanceType* type, uint32_t line, uint32_t column);
TypedExpr* CreateTypedFloatLitExpr(double val, LanceType* type, uint32_t line, uint32_t column);
TypedExpr* CreateTypedBoolLitExpr(bool val, LanceType* type, uint32_t line, uint32_t column);
TypedExpr* CreateTypedStringLitExpr(const char* val, LanceType* type, uint32_t line, uint32_t column);
TypedExpr* CreateTypedVarExpr(const char* varName, LanceType* type, uint32_t line, uint32_t column);
TypedExpr* CreateTypedCallExpr(TypedExpr* callee, TypedExpr* argument, LanceType* type, uint32_t line, uint32_t column);
TypedExpr* CreateTypedFieldAccessExpr(TypedExpr* target, const char* fieldName, size_t fieldIndex, LanceType* type, uint32_t line, uint32_t column);
TypedExpr* CreateTypedStructInitExpr(LanceType* structType, TypedFieldValue* fields, size_t fieldCount, uint32_t line, uint32_t column);

// Memory deallocation
void FreeTypedExpr(TypedExpr* expr);
void FreeTypedDecl(TypedDecl* decl);
void FreeTypedModule(TypedModule* module);

#endif // LANCE_TYPED_AST_H
