#include "typed_ast.h"

#include <stdlib.h>

static TypedExpr* AllocateTypedExprNode(TypedExprKind kind, LanceType* type, uint32_t line, uint32_t column) {
    TypedExpr* expr = (TypedExpr*)calloc(1, sizeof(TypedExpr));
    if (expr) {
        expr->kind = kind;
        expr->type = type;
        expr->line = line;
        expr->column = column;
    }
    return expr;
}

TypedExpr* CreateTypedIntLitExpr(int64_t val, LanceType* type, uint32_t line, uint32_t column) {
    TypedExpr* expr = AllocateTypedExprNode(TYPED_EXPR_INT_LIT, type, line, column);
    if (!expr) return nullptr;
    expr->intVal = val;
    return expr;
}

TypedExpr* CreateTypedFloatLitExpr(double val, LanceType* type, uint32_t line, uint32_t column) {
    TypedExpr* expr = AllocateTypedExprNode(TYPED_EXPR_FLOAT_LIT, type, line, column);
    if (!expr) return nullptr;
    expr->floatVal = val;
    return expr;
}

TypedExpr* CreateTypedBoolLitExpr(bool val, LanceType* type, uint32_t line, uint32_t column) {
    TypedExpr* expr = AllocateTypedExprNode(TYPED_EXPR_BOOL_LIT, type, line, column);
    if (!expr) return nullptr;
    expr->boolVal = val;
    return expr;
}

TypedExpr* CreateTypedStringLitExpr(const char* val, LanceType* type, uint32_t line, uint32_t column) {
    TypedExpr* expr = AllocateTypedExprNode(TYPED_EXPR_STRING_LIT, type, line, column);
    if (!expr) return nullptr;
    expr->stringVal = val;
    return expr;
}

TypedExpr* CreateTypedVarExpr(const char* name, const SlotRef slot, LanceType* type, uint32_t line, uint32_t column) {
    TypedExpr* expr = AllocateTypedExprNode(TYPED_EXPR_VAR, type, line, column);
    if (!expr) return nullptr;
    expr->var.name = name;
    expr->var.slot = slot;
    return expr;
}

TypedExpr* CreateTypedCallExpr(TypedExpr* callee, TypedExpr* argument, LanceType* type, uint32_t line, uint32_t column) {
    TypedExpr* expr = AllocateTypedExprNode(TYPED_EXPR_CALL, type, line, column);
    if (!expr) return nullptr;
    expr->call.callee = callee;
    expr->call.argument = argument;
    const LanceType* calleeType = callee ? callee->type : nullptr;
    expr->call.lazyArgument = calleeType && calleeType->kind == TYPE_FUNCTION && calleeType->function.lazyParam;
    return expr;
}

TypedExpr* CreateTypedLetExpr(size_t slot, TypedExpr* value, TypedExpr* body, uint32_t line, uint32_t column) {
    TypedExpr* expr = AllocateTypedExprNode(TYPED_EXPR_LET, body ? body->type : nullptr, line, column);
    if (!expr) return nullptr;
    expr->let.slot = slot;
    expr->let.value = value;
    expr->let.body = body;
    return expr;
}

TypedExpr* CreateTypedFieldAccessExpr(TypedExpr* target, const char* fieldName, size_t fieldIndex, LanceType* type, uint32_t line, uint32_t column) {
    TypedExpr* expr = AllocateTypedExprNode(TYPED_EXPR_FIELD_ACCESS, type, line, column);
    if (!expr) return nullptr;
    expr->fieldAccess.target = target;
    expr->fieldAccess.fieldName = fieldName;
    expr->fieldAccess.fieldIndex = fieldIndex;
    return expr;
}

TypedExpr* CreateTypedStructInitExpr(LanceType* structType, TypedFieldValue* fields, size_t fieldCount, uint32_t line, uint32_t column) {
    TypedExpr* expr = AllocateTypedExprNode(TYPED_EXPR_STRUCT_INIT, structType, line, column);
    if (!expr) return nullptr;
    expr->structInit.structType = structType;
    expr->structInit.fields = fields;
    expr->structInit.fieldCount = fieldCount;
    return expr;
}

void FreeTypedExpr(TypedExpr* expr) {
    if (!expr) return;

    switch (expr->kind) {
        case TYPED_EXPR_INT_LIT:
        case TYPED_EXPR_FLOAT_LIT:
        case TYPED_EXPR_BOOL_LIT:
        case TYPED_EXPR_STRING_LIT:
        case TYPED_EXPR_VAR:
            break;

        case TYPED_EXPR_CALL:
            FreeTypedExpr(expr->call.callee);
            FreeTypedExpr(expr->call.argument);
            break;

        case TYPED_EXPR_LET:
            FreeTypedExpr(expr->let.value);
            FreeTypedExpr(expr->let.body);
            break;

        case TYPED_EXPR_FIELD_ACCESS:
            FreeTypedExpr(expr->fieldAccess.target);
            break;

        case TYPED_EXPR_STRUCT_INIT:
            if (expr->structInit.fields) {
                for (size_t i = 0; i < expr->structInit.fieldCount; i++) {
                    FreeTypedExpr(expr->structInit.fields[i].value);
                }
                free(expr->structInit.fields);
            }
            break;
    }

    free(expr);
}

void FreeTypedDecl(TypedDecl* decl) {
    if (!decl) return;
    if (decl->params) {
        free((void*)decl->params);
        decl->params = nullptr;
    }
    FreeTypedExpr(decl->body);
    decl->body = nullptr;
}

void FreeTypedModule(TypedModule* module) {
    if (!module) return;
    if (module->declarations) {
        for (size_t i = 0; i < module->count; i++) {
            FreeTypedDecl(&module->declarations[i]);
        }
        free(module->declarations);
    }
    free(module);
}
