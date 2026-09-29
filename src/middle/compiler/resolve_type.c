#include "compiler_internal.h"

#include <string.h>

LanceType* NewFunctionType(Compiler* compiler, LanceType* paramType, LanceType* returnType) {
    return CreateFunctionType(&compiler->types, paramType, returnType);
}

LanceType* NewBinaryOperatorType(Compiler* compiler, LanceType* valueType, LanceType* resultType) {
    return NewFunctionType(compiler, valueType, NewFunctionType(compiler, valueType, resultType));
}

LanceType* LookupTypeName(const char* name, const SymbolTable* scope) {
    LanceType* primitive = GetPrimitiveTypeByName(name);
    if (primitive) return primitive;

    const Symbol* symbol = SymbolTableLookup(scope, name);
    return IsTypeLevelSymbol(symbol) ? symbol->type : nullptr;
}

LanceType* ResolveAstType(Compiler* compiler, const AstType* astType, const SymbolTable* scope) {
    if (!astType) return nullptr;

    switch (astType->kind) {
        case AST_TYPE_NAMED: {
            LanceType* type = LookupTypeName(astType->named.name, scope);
            if (!type) {
                CompilerError(compiler, astType->line, astType->column, "Unknown type '%s'", astType->named.name);
            }
            return type;
        }

        case AST_TYPE_FUNCTION: {
            LanceType* paramType = ResolveAstType(compiler, astType->function.paramType, scope);
            LanceType* returnType = ResolveAstType(compiler, astType->function.returnType, scope);
            if (!paramType || !returnType) return nullptr;
            return NewFunctionType(compiler, paramType, returnType);
        }

        case AST_TYPE_STRUCT: {
            const size_t count = astType->structType.fieldCount;
            StructFieldType* fields = ARENA_ARRAY(&compiler->types, StructFieldType, count);
            for (size_t i = 0; i < count; i++) {
                fields[i].name = astType->structType.fields[i].name;
                fields[i].type = ResolveAstType(compiler, astType->structType.fields[i].type, scope);
            }
            return CreateStructType(&compiler->types, nullptr, fields, count);
        }

        case AST_TYPE_CONSTRAINED: {
            SymbolTable* genericScope = CreateSymbolTable((SymbolTable*)scope);

            for (size_t i = 0; i < astType->constrained.constraintCount; i++) {
                const char* param = astType->constrained.constraints[i].typeParam;
                if (!SymbolTableLookup(genericScope, param)) {
                    SymbolTableInsert(genericScope, param, SYMBOL_TYPE, GetTypeType(), nullptr);
                }
            }

            LanceType* resolved = ResolveAstType(compiler, astType->constrained.targetType, genericScope);
            FreeSymbolTable(genericScope);
            return resolved;
        }
    }

    return nullptr;
}

LanceType* InstantiateGenericType(Compiler* compiler, const AstType* astType, const char* paramName,
                                  const LanceType* concreteType, const SymbolTable* scope) {
    if (!astType) return nullptr;

    SymbolTable* instScope = CreateSymbolTable((SymbolTable*)scope);
    SymbolTableInsert(instScope, paramName, SYMBOL_TYPE, (LanceType*)concreteType, nullptr);

    LanceType* resolved = ResolveAstType(compiler, astType, instScope);
    FreeSymbolTable(instScope);
    return resolved;
}

bool AstTypeIsType(const AstType* astType) {
    if (!astType) return false;
    if (astType->kind == AST_TYPE_NAMED) {
        return GetPrimitiveTypeByName(astType->named.name) == GetTypeType();
    }
    if (astType->kind == AST_TYPE_FUNCTION) {
        return AstTypeIsType(astType->function.returnType);
    }
    return false;
}

LanceType* EvalTypeExpr(Compiler* compiler, const AstExpr* expr, const SymbolTable* scope) {
    if (!expr) return nullptr;

    switch (expr->kind) {
        case AST_EXPR_TYPE:
            return ResolveAstType(compiler, expr->typeExpr, scope);

        case AST_EXPR_IDENT:
            return LookupTypeName(expr->identName, scope);

        case AST_EXPR_CALL: {
            // Type function application, e.g. `Vec f32`: evaluate the type
            // function's body with its parameter bound to the argument.
            if (expr->call.callee->kind != AST_EXPR_IDENT) return nullptr;

            const Symbol* callee = SymbolTableLookup(scope, expr->call.callee->identName);
            if (!callee || callee->kind != SYMBOL_TYPE_FUNCTION || !callee->valueDecl) return nullptr;

            LanceType* argType = EvalTypeExpr(compiler, expr->call.argument, scope);
            const AstDecl* definition = callee->valueDecl;

            // The body is evaluated where the type function is defined.
            const CompilerModule* module = ModuleOfDecl(compiler, definition);
            SymbolTable* typeScope = CreateSymbolTable(module ? module->scope : compiler->builtins);
            if (definition->paramCount > 0) {
                SymbolTableInsert(typeScope, definition->params[0], SYMBOL_TYPE, argType, nullptr);
            }

            LanceType* resultType = EvalTypeExpr(compiler, definition->body, typeScope);
            FreeSymbolTable(typeScope);
            return resultType;
        }

        default:
            return nullptr;
    }
}
