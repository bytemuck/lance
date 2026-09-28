#include "ast.h"

#include "memory.h"

#include <stdlib.h>
#include <string.h>

static AstType* AllocateTypeNode(const AstTypeKind kind, const uint32_t line, const uint32_t column) {
    AstType* node = (AstType*)calloc(1, sizeof(AstType));
    if (node) {
        node->kind = kind;
        node->line = line;
        node->column = column;
    }
    return node;
}

AstType* CreateFunctionTypeAst(AstType* paramType, AstType* returnType, uint32_t line, uint32_t column) {
    AstType* node = AllocateTypeNode(AST_TYPE_FUNCTION, line, column);
    if (!node) return nullptr;
    node->function.paramType = paramType;
    node->function.returnType = returnType;
    return node;
}

AstType* CreateConstrainedTypeAst(AstConstraint* constraints, const size_t count, AstType* targetType, const uint32_t line, const uint32_t column) {
    AstType* node = AllocateTypeNode(AST_TYPE_CONSTRAINED, line, column);
    if (!node) return nullptr;
    node->constrained.constraints = constraints;
    node->constrained.constraintCount = count;
    node->constrained.targetType = targetType;
    return node;
}

void FreeTypeAst(AstType* type) {
    if (!type) return;

    switch (type->kind) {
        case AST_TYPE_NAMED:
            break;
        case AST_TYPE_FUNCTION:
            FreeTypeAst(type->function.paramType);
            FreeTypeAst(type->function.returnType);
            break;
        case AST_TYPE_STRUCT:
            if (type->structType.fields) {
                for (size_t i = 0; i < type->structType.fieldCount; i++) {
                    FreeTypeAst(type->structType.fields[i].type);
                }
                free(type->structType.fields);
            }
            break;
        case AST_TYPE_CONSTRAINED:
    		if (type->constrained.constraints) {
    			FREE_ARRAY(AstConstraint, type->constrained.constraints, type->constrained.constraintCount);
    		}
    		FreeTypeAst(type->constrained.targetType);
            break;
    }

    free(type);
}

AstType* CreateNamedTypeAst(const char* name, const uint32_t line, const uint32_t column) {
    AstType* const node = AllocateTypeNode(AST_TYPE_NAMED, line, column);
    if (!node) return nullptr;
    node->named.name = name;
    return node;
}

AstType* CreateStructTypeAst(AstFieldDecl* fields, const size_t fieldCount, const uint32_t line, const uint32_t column) {
    AstType* const node = AllocateTypeNode(AST_TYPE_STRUCT, line, column);
    if (!node) return nullptr;
    node->structType.fields = fields;
    node->structType.fieldCount = fieldCount;
    return node;
}

static AstExpr* AllocateExprNode(const AstExprKind kind, const uint32_t line, const uint32_t column) {
    AstExpr* const node = (AstExpr*)calloc(1, sizeof(AstExpr));
    if (node) {
        node->kind = kind;
        node->line = line;
        node->column = column;
    }
    return node;
}

AstExpr* CreateIntLitExpr(const int64_t value, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(AST_EXPR_INT_LIT, line, column);
    if (!node) return nullptr;
    node->intVal = value;
    return node;
}

AstExpr* CreateFloatLitExpr(const double value, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(AST_EXPR_FLOAT_LIT, line, column);
    if (!node) return nullptr;
    node->floatVal = value;
    return node;
}

AstExpr* CreateStringLitExpr(const char* value, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(AST_EXPR_STRING_LIT, line, column);
    if (!node) return nullptr;
    node->stringVal = value;
    return node;
}

AstExpr* CreateBoolLitExpr(const bool value, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(AST_EXPR_BOOL_LIT, line, column);
    if (!node) return nullptr;
    node->boolVal = value;
    return node;
}

AstExpr* CreateIdentExpr(const char* name, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(AST_EXPR_IDENT, line, column);
    if (!node) return nullptr;
    node->identName = name;
    return node;
}

AstExpr* CreateTypeExpr(AstType* typeExpr, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(AST_EXPR_TYPE, line, column);
    if (!node) return nullptr;
    node->typeExpr = typeExpr;
    return node;
}

AstExpr* CreateBinaryExpr(const TokenType op, AstExpr* left, AstExpr* right, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(AST_EXPR_BINARY, line, column);
    if (!node) return nullptr;
    node->binary.op = op;
    node->binary.left = left;
    node->binary.right = right;
    return node;
}

AstExpr* CreateCallExpr(AstExpr* callee, AstExpr* args, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(AST_EXPR_CALL, line, column);
    if (!node) return nullptr;
    node->call.callee = callee;
    node->call.argument = args;
    return node;
}

AstExpr* CreateFieldAccessExpr(AstExpr* base, const char* fieldName, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(AST_EXPR_FIELD_ACCESS, line, column);
    if (!node) return nullptr;
    node->fieldAccess.target = base;
    node->fieldAccess.fieldName = fieldName;
    return node;
}

AstExpr* CreateStructValueExpr(AstFieldValue* fields, const size_t fieldCount, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(AST_EXPR_STRUCT_VALUE, line, column);
    if (!node) return nullptr;
    node->structValue.fields = fields;
    node->structValue.fieldCount = fieldCount;
    return node;
}

AstExpr* CreateComptimeExpr(AstExpr* expr, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(AST_EXPR_COMPTIME, line, column);
    if (!node) return nullptr;
    node->comptime.inner = expr;
    return node;
}

void FreeExprAst(AstExpr* expr) {
    if (!expr) return;

    switch (expr->kind) {
        case AST_EXPR_INT_LIT:
        case AST_EXPR_FLOAT_LIT:
        case AST_EXPR_BOOL_LIT:
        case AST_EXPR_STRING_LIT:
        case AST_EXPR_IDENT:
            break;
        case AST_EXPR_TYPE:
            FreeTypeAst(expr->typeExpr);
            break;
        case AST_EXPR_BINARY:
            FreeExprAst(expr->binary.left);
            FreeExprAst(expr->binary.right);
            break;
        case AST_EXPR_CALL:
            FreeExprAst(expr->call.callee);
            FreeExprAst(expr->call.argument);
            break;
        case AST_EXPR_FIELD_ACCESS:
            FreeExprAst(expr->fieldAccess.target);
            break;
        case AST_EXPR_STRUCT_VALUE:
            if (expr->structValue.fields) {
                for (size_t i = 0; i < expr->structValue.fieldCount; i++) {
                    FreeExprAst(expr->structValue.fields[i].value);
                }
                free(expr->structValue.fields);
            }
            break;
        case AST_EXPR_COMPTIME:
            FreeExprAst(expr->comptime.inner);
            break;
    }

    free(expr);
}

void FreeDeclAst(AstDecl* decl) {
    if (!decl) return;

    if (decl->params) {
        free((void*)decl->params);
        decl->params = nullptr;
    }

    switch (decl->kind) {
        case AST_DECL_TYPE_ANNOTATION:
            FreeTypeAst(decl->typeAnnotation);
            decl->typeAnnotation = nullptr;
            break;
        case AST_DECL_BINDING:
            FreeExprAst(decl->body);
            decl->body = nullptr;
            break;
    }
}

void FreeModuleAst(AstModule* module) {
    if (!module) return;

    if (module->declarations) {
        for (size_t i = 0; i < module->count; i++) {
            FreeDeclAst(&module->declarations[i]);
        }

        free(module->declarations);
    }

    free(module);
}
