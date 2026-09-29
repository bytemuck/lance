#include "ast.h"

#include <string.h>

void* GrowAstArray(Arena* arena, const void* items, const size_t oldCount, const size_t newCount, const size_t itemSize) {
    void* grown = ArenaAlloc(arena, newCount * itemSize);
    if (oldCount) memcpy(grown, items, oldCount * itemSize);
    return grown;
}

static AstType* AllocateTypeNode(Arena* arena, const AstTypeKind kind, const uint32_t line, const uint32_t column) {
    AstType* node = ARENA_NEW(arena, AstType);
    node->kind = kind;
    node->line = line;
    node->column = column;
    return node;
}

AstType* CreateFunctionTypeAst(Arena* arena, AstType* paramType, AstType* returnType, uint32_t line, uint32_t column) {
    AstType* node = AllocateTypeNode(arena, AST_TYPE_FUNCTION, line, column);
    node->function.paramType = paramType;
    node->function.returnType = returnType;
    return node;
}

AstType* CreateConstrainedTypeAst(Arena* arena, AstConstraint* constraints, const size_t count, AstType* targetType, const uint32_t line, const uint32_t column) {
    AstType* node = AllocateTypeNode(arena, AST_TYPE_CONSTRAINED, line, column);
    node->constrained.constraints = constraints;
    node->constrained.constraintCount = count;
    node->constrained.targetType = targetType;
    return node;
}

AstType* CloneAstType(Arena* arena, const AstType* type) {
    if (!type) return nullptr;

    switch (type->kind) {
        case AST_TYPE_NAMED:
            return CreateNamedTypeAst(arena, type->named.name, type->line, type->column);

        case AST_TYPE_FUNCTION:
            return CreateFunctionTypeAst(arena,
                CloneAstType(arena, type->function.paramType),
                CloneAstType(arena, type->function.returnType),
                type->line,
                type->column
            );

        case AST_TYPE_STRUCT: {
            const size_t count = type->structType.fieldCount;
            AstFieldDecl* fields = nullptr;
            if (count > 0) {
                fields = ARENA_ARRAY(arena, AstFieldDecl, count);
                for (size_t i = 0; i < count; i++) {
                    fields[i].name = type->structType.fields[i].name;
                    fields[i].type = CloneAstType(arena, type->structType.fields[i].type);
                }
            }
            return CreateStructTypeAst(arena, fields, count, type->line, type->column);
        }

        case AST_TYPE_CONSTRAINED: {
            const size_t count = type->constrained.constraintCount;
            AstConstraint* constraints = nullptr;
            if (count > 0) {
                constraints = ARENA_ARRAY(arena, AstConstraint, count);
                for (size_t i = 0; i < count; i++) {
                    constraints[i].interfaceName = type->constrained.constraints[i].interfaceName;
                    constraints[i].typeParam = type->constrained.constraints[i].typeParam;
                }
            }
            return CreateConstrainedTypeAst(arena,
                constraints,
                count,
                CloneAstType(arena, type->constrained.targetType),
                type->line,
                type->column
            );
        }
    }

    return nullptr;
}

AstType* CreateNamedTypeAst(Arena* arena, const char* name, const uint32_t line, const uint32_t column) {
    AstType* const node = AllocateTypeNode(arena, AST_TYPE_NAMED, line, column);
    node->named.name = name;
    return node;
}

AstType* CreateStructTypeAst(Arena* arena, AstFieldDecl* fields, const size_t fieldCount, const uint32_t line, const uint32_t column) {
    AstType* const node = AllocateTypeNode(arena, AST_TYPE_STRUCT, line, column);
    node->structType.fields = fields;
    node->structType.fieldCount = fieldCount;
    return node;
}

static AstExpr* AllocateExprNode(Arena* arena, const AstExprKind kind, const uint32_t line, const uint32_t column) {
    AstExpr* const node = ARENA_NEW(arena, AstExpr);
    node->kind = kind;
    node->line = line;
    node->column = column;
    return node;
}

AstExpr* CreateIntLitExpr(Arena* arena, const int64_t value, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(arena, AST_EXPR_INT_LIT, line, column);
    node->intVal = value;
    return node;
}

AstExpr* CreateFloatLitExpr(Arena* arena, const double value, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(arena, AST_EXPR_FLOAT_LIT, line, column);
    node->floatVal = value;
    return node;
}

AstExpr* CreateStringLitExpr(Arena* arena, const char* value, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(arena, AST_EXPR_STRING_LIT, line, column);
    node->stringVal = value;
    return node;
}

AstExpr* CreateBoolLitExpr(Arena* arena, const bool value, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(arena, AST_EXPR_BOOL_LIT, line, column);
    node->boolVal = value;
    return node;
}

AstExpr* CreateIdentExpr(Arena* arena, const char* name, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(arena, AST_EXPR_IDENT, line, column);
    node->identName = name;
    return node;
}

AstExpr* CreateTypeExpr(Arena* arena, AstType* typeExpr, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(arena, AST_EXPR_TYPE, line, column);
    node->typeExpr = typeExpr;
    return node;
}

AstExpr* CreateCallExpr(Arena* arena, AstExpr* callee, AstExpr* args, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(arena, AST_EXPR_CALL, line, column);
    node->call.callee = callee;
    node->call.argument = args;
    return node;
}

AstExpr* CreateIfExpr(Arena* arena, AstExpr* condition, AstExpr* thenBranch, AstExpr* elseBranch, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(arena, AST_EXPR_IF, line, column);
    node->conditional.condition = condition;
    node->conditional.thenBranch = thenBranch;
    node->conditional.elseBranch = elseBranch;
    return node;
}

AstExpr* CreateLetExpr(Arena* arena, const char* name, AstExpr* value, AstExpr* body, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(arena, AST_EXPR_LET, line, column);
    node->let.name = name;
    node->let.value = value;
    node->let.body = body;
    return node;
}

AstExpr* CreateFieldAccessExpr(Arena* arena, AstExpr* base, const char* fieldName, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(arena, AST_EXPR_FIELD_ACCESS, line, column);
    node->fieldAccess.target = base;
    node->fieldAccess.fieldName = fieldName;
    return node;
}

AstExpr* CreateStructValueExpr(Arena* arena, AstFieldValue* fields, const size_t fieldCount, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(arena, AST_EXPR_STRUCT_VALUE, line, column);
    node->structValue.fields = fields;
    node->structValue.fieldCount = fieldCount;
    return node;
}

AstExpr* CreateComptimeExpr(Arena* arena, AstExpr* expr, const uint32_t line, const uint32_t column) {
    AstExpr* const node = AllocateExprNode(arena, AST_EXPR_COMPTIME, line, column);
    node->comptime.inner = expr;
    return node;
}
