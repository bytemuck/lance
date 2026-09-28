#include "compiler.h"
#include "memory.h"
#include "string_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LanceType* TrackType(Compiler* compiler, LanceType* type) {
    if (!type || type->id < 15) return type;
    if (compiler->allocatedTypes.count >= compiler->allocatedTypes.capacity) {
        const size_t oldCap = compiler->allocatedTypes.capacity;
        compiler->allocatedTypes.capacity = GROW_CAPACITY(oldCap);
        compiler->allocatedTypes.types = GROW_ARRAY(LanceType*, compiler->allocatedTypes.types, oldCap, compiler->allocatedTypes.capacity);
    }
    compiler->allocatedTypes.types[compiler->allocatedTypes.count++] = type;
    return type;
}

void InitializeCompiler(Compiler* compiler) {
    compiler->globals = CreateSymbolTable(nullptr);
    compiler->instances = (CompilerInstanceRegistry){0};
    compiler->allocatedTypes = (CompilerTypeRegistry){0};
    compiler->typedModule = nullptr;
    compiler->hadError = false;

    // Register prelude operator type signatures
    LanceType* f32_binop = TrackType(compiler, CreateFunctionType(GetTypeF32(), TrackType(compiler, CreateFunctionType(GetTypeF32(), GetTypeF32()))));
    LanceType* i32_binop = TrackType(compiler, CreateFunctionType(GetTypeI32(), TrackType(compiler, CreateFunctionType(GetTypeI32(), GetTypeI32()))));

    SymbolTableInsert(compiler->globals, "+", f32_binop, nullptr, false);
    SymbolTableInsert(compiler->globals, "-", f32_binop, nullptr, false);
    SymbolTableInsert(compiler->globals, "*", f32_binop, nullptr, false);
    SymbolTableInsert(compiler->globals, "/", f32_binop, nullptr, false);

    // Register boolean comparisons
    LanceType* f32_cmp = TrackType(compiler, CreateFunctionType(GetTypeF32(), TrackType(compiler, CreateFunctionType(GetTypeF32(), GetTypeBool()))));
    SymbolTableInsert(compiler->globals, "==", f32_cmp, nullptr, false);
    SymbolTableInsert(compiler->globals, "!=", f32_cmp, nullptr, false);
    SymbolTableInsert(compiler->globals, "<", f32_cmp, nullptr, false);
    SymbolTableInsert(compiler->globals, "<=", f32_cmp, nullptr, false);
    SymbolTableInsert(compiler->globals, ">", f32_cmp, nullptr, false);
    SymbolTableInsert(compiler->globals, ">=", f32_cmp, nullptr, false);
}

void FreeCompiler(Compiler* compiler) {
    FreeSymbolTable(compiler->globals);
    if (compiler->instances.entries) {
        FREE_ARRAY(CompilerInstanceEntry, compiler->instances.entries, compiler->instances.capacity);
        compiler->instances.entries = nullptr;
    }
    for (size_t i = 0; i < compiler->allocatedTypes.count; i++) {
        LanceType* t = compiler->allocatedTypes.types[i];
        if (t && t->id >= 15) {
            if (t->kind == TYPE_STRUCT && t->structType.fields) {
                free(t->structType.fields);
                t->structType.fields = nullptr;
            }
            free(t);
        }
    }
    if (compiler->allocatedTypes.types) {
        FREE_ARRAY(LanceType*, compiler->allocatedTypes.types, compiler->allocatedTypes.capacity);
        compiler->allocatedTypes.types = nullptr;
    }
}

static LanceType* ResolveAstType(Compiler* compiler, const AstType* astType, const SymbolTable* scope) {
    if (!astType) return nullptr;

    switch (astType->kind) {
        case AST_TYPE_NAMED: {
            LanceType* primitive = GetPrimitiveTypeByName(astType->named.name);
            if (primitive) return primitive;

            const Symbol* symbol = SymbolTableLookup(scope, astType->named.name);
            if (symbol && symbol->type) {
                return symbol->type;
            }

            fprintf(stderr, "[%u:%u] Type Error: Unknown type '%s'\n", astType->line, astType->column, astType->named.name);
            compiler->hadError = true;
            return nullptr;
        }

        case AST_TYPE_FUNCTION: {
            LanceType* paramType = ResolveAstType(compiler, astType->function.paramType, scope);
            LanceType* returnType = ResolveAstType(compiler, astType->function.returnType, scope);
            if (!paramType || !returnType) return nullptr;
            return TrackType(compiler, CreateFunctionType(paramType, returnType));
        }

        case AST_TYPE_STRUCT: {
            StructFieldType* fields = (StructFieldType*)malloc(astType->structType.fieldCount * sizeof(StructFieldType));
            for (size_t i = 0; i < astType->structType.fieldCount; i++) {
                fields[i].name = astType->structType.fields[i].name;
                fields[i].type = ResolveAstType(compiler, astType->structType.fields[i].type, scope);
            }
            return TrackType(compiler, CreateStructType(nullptr, fields, astType->structType.fieldCount));
        }

        case AST_TYPE_CONSTRAINED: {
            if (SymbolTableLookup(scope, astType->constrained.typeParam)) {
                return ResolveAstType(compiler, astType->constrained.targetType, scope);
            }

            SymbolTable* genericScope = CreateSymbolTable((SymbolTable*)scope);
            SymbolTableInsert(genericScope, astType->constrained.typeParam, GetTypeType(), nullptr, false);

            LanceType* resolved = ResolveAstType(compiler, astType->constrained.targetType, genericScope);
            FreeSymbolTable(genericScope);
            return resolved;
        }
    }

    return nullptr;
}

static LanceType* InstantiateGenericType(Compiler* compiler, const AstType* astType, const char* paramName, const LanceType* concreteType, const SymbolTable* scope) {
    if (!astType) return nullptr;

    SymbolTable* instScope = CreateSymbolTable((SymbolTable*)scope);
    SymbolTableInsert(instScope, paramName, (LanceType*)concreteType, nullptr, false);

    LanceType* resolved = ResolveAstType(compiler, astType, instScope);
    FreeSymbolTable(instScope);
    return resolved;
}

static void RegisterInstance(CompilerInstanceRegistry* reg, const char* ifaceName, const LanceType* type, AstDecl* decl) {
    if (reg->count >= reg->capacity) {
        const size_t oldCap = reg->capacity;
        reg->capacity = GROW_CAPACITY(oldCap);
        reg->entries = GROW_ARRAY(CompilerInstanceEntry, reg->entries, oldCap, reg->capacity);
    }

    reg->entries[reg->count++] = (CompilerInstanceEntry){
        .interfaceName = InternCString(ifaceName),
        .targetType = type,
        .instanceDecl = decl
    };
}

static AstDecl* LookupInstance(const CompilerInstanceRegistry* reg, const char* ifaceName, const LanceType* type) {
    const char* internedIface = InternCString(ifaceName);
    for (size_t i = 0; i < reg->count; i++) {
        if (reg->entries[i].interfaceName == internedIface && TypesAreEqual(reg->entries[i].targetType, type)) {
            return reg->entries[i].instanceDecl;
        }
    }
    return nullptr;
}

static bool AstTypeIsType(const AstType* astType) {
    if (!astType) return false;
    if (astType->kind == AST_TYPE_NAMED) {
        return (strcmp(astType->named.name, "type") == 0 || strcmp(astType->named.name, "Type") == 0);
    }
    if (astType->kind == AST_TYPE_FUNCTION) {
        return AstTypeIsType(astType->function.returnType);
    }
    return false;
}

static LanceType* EvalTypeExpr(Compiler* compiler, const AstExpr* expr, const SymbolTable* scope) {
    if (!expr) return nullptr;

    if (expr->kind == AST_EXPR_TYPE) {
        return ResolveAstType(compiler, expr->typeExpr, scope);
    }

    if (expr->kind == AST_EXPR_IDENT) {
        LanceType* prim = GetPrimitiveTypeByName(expr->identName);
        if (prim) return prim;

        const Symbol* sym = SymbolTableLookup(scope, expr->identName);
        if (sym && sym->type) return sym->type;
    }

    if (expr->kind == AST_EXPR_CALL) {
        LanceType* argType = EvalTypeExpr(compiler, expr->call.argument, scope);

        if (expr->call.callee->kind == AST_EXPR_IDENT) {
            const Symbol* calleeSym = SymbolTableLookup(scope, expr->call.callee->identName);
            if (calleeSym && calleeSym->valueDecl && calleeSym->valueDecl->kind == AST_DECL_BINDING) {
                const AstDecl* fnDecl = calleeSym->valueDecl;

                SymbolTable* typeScope = CreateSymbolTable(compiler->globals);
                if (fnDecl->paramCount > 0) {
                    SymbolTableInsert(typeScope, fnDecl->params[0], argType, nullptr, false);
                }

                LanceType* resultType = EvalTypeExpr(compiler, fnDecl->body, typeScope);
                FreeSymbolTable(typeScope);
                return resultType;
            }
        }
    }

    return nullptr;
}

static const AstExpr* FindInstanceMethod(const AstDecl* instanceDecl, const char* methodName) {
    if (!instanceDecl || !instanceDecl->body || instanceDecl->body->kind != AST_EXPR_STRUCT_VALUE) {
        return nullptr;
    }

    const size_t fieldCount = instanceDecl->body->structValue.fieldCount;
    const AstFieldValue* fields = instanceDecl->body->structValue.fields;
    for (size_t i = 0; i < fieldCount; i++) {
        if (strcmp(fields[i].name, methodName) == 0) {
            return fields[i].value;
        }
    }
    return nullptr;
}

static AstExpr* CloneAndSpecializeAstExpr(const AstExpr* expr, const char* typeParam, const AstDecl* instanceDecl) {
    if (!expr) return nullptr;

    switch (expr->kind) {
        case AST_EXPR_FIELD_ACCESS: {
            if (expr->fieldAccess.target->kind == AST_EXPR_IDENT &&
                strcmp(expr->fieldAccess.target->identName, typeParam) == 0) {
                const AstExpr* methodExpr = FindInstanceMethod(instanceDecl, expr->fieldAccess.fieldName);
                if (methodExpr) {
                    return CloneAndSpecializeAstExpr(methodExpr, typeParam, instanceDecl);
                }
            }
            return CreateFieldAccessExpr(
                CloneAndSpecializeAstExpr(expr->fieldAccess.target, typeParam, instanceDecl),
                expr->fieldAccess.fieldName,
                expr->line, expr->column
            );
        }

        case AST_EXPR_CALL:
            return CreateCallExpr(
                CloneAndSpecializeAstExpr(expr->call.callee, typeParam, instanceDecl),
                CloneAndSpecializeAstExpr(expr->call.argument, typeParam, instanceDecl),
                expr->line, expr->column
            );

        case AST_EXPR_STRUCT_VALUE: {
            size_t count = expr->structValue.fieldCount;
            AstFieldValue* fields = ALLOCATE(AstFieldValue, count);
            for (size_t i = 0; i < count; i++) {
                fields[i].name = expr->structValue.fields[i].name;
                fields[i].value = CloneAndSpecializeAstExpr(expr->structValue.fields[i].value, typeParam, instanceDecl);
            }
            return CreateStructValueExpr(fields, count, expr->line, expr->column);
        }

        case AST_EXPR_INT_LIT:   return CreateIntLitExpr(expr->intVal, expr->line, expr->column);
        case AST_EXPR_FLOAT_LIT: return CreateFloatLitExpr(expr->floatVal, expr->line, expr->column);
        case AST_EXPR_BOOL_LIT:  return CreateBoolLitExpr(expr->boolVal, expr->line, expr->column);
        case AST_EXPR_STRING_LIT: return CreateStringLitExpr(expr->stringVal, expr->line, expr->column);
        case AST_EXPR_IDENT:     return CreateIdentExpr(expr->identName, expr->line, expr->column);
        case AST_EXPR_TYPE:      return CreateTypeExpr(expr->typeExpr, expr->line, expr->column);
        case AST_EXPR_COMPTIME:  return CreateComptimeExpr(CloneAndSpecializeAstExpr(expr->comptime.inner, typeParam, instanceDecl), expr->line, expr->column);

        default:
            return nullptr;
    }
}

static TypedExpr* LowerExpr(Compiler* compiler, const AstExpr* expr, const SymbolTable* scope, LanceType* expectedType);

static void AppendTypedDecl(TypedModule* module, TypedDecl decl) {
    if (module->count >= module->capacity) {
        const size_t oldCap = module->capacity;
        module->capacity = GROW_CAPACITY(oldCap);
        module->declarations = GROW_ARRAY(TypedDecl, module->declarations, oldCap, module->capacity);
    }
    module->declarations[module->count++] = decl;
}

static const char* MonomorphizeGenericFunction(Compiler* compiler, const Symbol* symbol, LanceType* concreteArgType, const SymbolTable* scope, uint32_t line, uint32_t column) {
    const AstDecl* typeDecl = symbol->typeDecl;
    const AstDecl* valueDecl = symbol->valueDecl;
    const AstType* constrType = typeDecl->typeAnnotation;
    const char* ifaceName = constrType->constrained.interfaceName;
    const char* symName = symbol->name;

    const AstDecl* instance = LookupInstance(&compiler->instances, ifaceName, concreteArgType);
    if (!instance) {
        fprintf(stderr, "[%u:%u] Type Error: Type '%s' does not implement interface '%s'\n",
                line, column, TypeToString(concreteArgType), ifaceName);
        compiler->hadError = true;
        return nullptr;
    }

    char specName[128];
    snprintf(specName, sizeof(specName), "%s$%s", symName, TypeToString(concreteArgType));
    const char* internedSpecName = InternCString(specName);

    Symbol* existingSpec = SymbolTableLookup(compiler->globals, internedSpecName);
    if (!existingSpec) {
        AstExpr* specializedBody = CloneAndSpecializeAstExpr(valueDecl->body, constrType->constrained.typeParam, instance);
        LanceType* specSig = InstantiateGenericType(compiler, constrType->constrained.targetType, constrType->constrained.typeParam, concreteArgType, scope);

        SymbolTableInsert(compiler->globals, internedSpecName, specSig, nullptr, false);

        // Lower the specialized body in a parameter scope
        SymbolTable* paramScope = CreateSymbolTable(compiler->globals);
        LanceType* curSig = specSig;
        for (size_t p = 0; p < valueDecl->paramCount; p++) {
            if (curSig && curSig->kind == TYPE_FUNCTION) {
                SymbolTableInsert(paramScope, valueDecl->params[p], curSig->function.paramType, nullptr, false);
                curSig = curSig->function.returnType;
            }
        }

        TypedExpr* loweredBody = LowerExpr(compiler, specializedBody, paramScope, curSig);
        FreeSymbolTable(paramScope);

        // Clean up specializedBody
        FreeExprAst(specializedBody);

        const char** paramsCopy = nullptr;
        if (valueDecl->paramCount > 0) {
            paramsCopy = ALLOCATE(const char*, valueDecl->paramCount);
            for (size_t p = 0; p < valueDecl->paramCount; p++) {
                paramsCopy[p] = valueDecl->params[p];
            }
        }

        TypedDecl specDecl = {
            .name = internedSpecName,
            .type = specSig,
            .params = paramsCopy,
            .paramCount = valueDecl->paramCount,
            .body = loweredBody,
            .line = line,
            .column = column
        };

        AppendTypedDecl(compiler->typedModule, specDecl);
        existingSpec = SymbolTableLookup(compiler->globals, internedSpecName);
    }

    return existingSpec ? existingSpec->name : nullptr;
}

static TypedExpr* LowerExpr(Compiler* compiler, const AstExpr* expr, const SymbolTable* scope, LanceType* expectedType) {
    if (!expr) return nullptr;

    switch (expr->kind) {
        case AST_EXPR_INT_LIT:
            return CreateTypedIntLitExpr(expr->intVal, GetTypeI32(), expr->line, expr->column);

        case AST_EXPR_FLOAT_LIT:
            return CreateTypedFloatLitExpr(expr->floatVal, GetTypeF32(), expr->line, expr->column);

        case AST_EXPR_BOOL_LIT:
            return CreateTypedBoolLitExpr(expr->boolVal, GetTypeBool(), expr->line, expr->column);

        case AST_EXPR_STRING_LIT:
            return CreateTypedStringLitExpr(expr->stringVal, GetTypeUnit(), expr->line, expr->column);

        case AST_EXPR_IDENT: {
            LanceType* prim = GetPrimitiveTypeByName(expr->identName);
            if (prim) {
                return CreateTypedVarExpr(expr->identName, GetTypeType(), expr->line, expr->column);
            }

            const Symbol* sym = SymbolTableLookup(scope, expr->identName);
            if (!sym) {
                fprintf(stderr, "[%u:%u] Type Error: Undefined identifier '%s'\n", expr->line, expr->column, expr->identName);
                compiler->hadError = true;
                return nullptr;
            }

            return CreateTypedVarExpr(sym->name, sym->type, expr->line, expr->column);
        }

        case AST_EXPR_FIELD_ACCESS: {
            TypedExpr* target = LowerExpr(compiler, expr->fieldAccess.target, scope, nullptr);
            if (!target || !target->type || target->type->kind != TYPE_STRUCT) {
                fprintf(stderr, "[%u:%u] Type Error: Field access '%s' on non-struct type\n",
                        expr->line, expr->column, expr->fieldAccess.fieldName);
                compiler->hadError = true;
                return nullptr;
            }

            for (size_t i = 0; i < target->type->structType.fieldCount; i++) {
                if (strcmp(target->type->structType.fields[i].name, expr->fieldAccess.fieldName) == 0) {
                    LanceType* fieldType = target->type->structType.fields[i].type;
                    return CreateTypedFieldAccessExpr(target, expr->fieldAccess.fieldName, i, fieldType, expr->line, expr->column);
                }
            }

            fprintf(stderr, "[%u:%u] Type Error: Struct has no field '%s'\n",
                    expr->line, expr->column, expr->fieldAccess.fieldName);
            compiler->hadError = true;
            return nullptr;
        }

        case AST_EXPR_STRUCT_VALUE: {
            LanceType* structType = expectedType;
            if (!structType || structType->kind != TYPE_STRUCT) {
                fprintf(stderr, "[%u:%u] Type Error: Cannot determine struct type for literal\n", expr->line, expr->column);
                compiler->hadError = true;
                return nullptr;
            }

            size_t count = expr->structValue.fieldCount;
            TypedFieldValue* fields = ALLOCATE(TypedFieldValue, count);

            for (size_t i = 0; i < count; i++) {
                const char* fName = expr->structValue.fields[i].name;
                LanceType* fExpectedType = nullptr;

                for (size_t s = 0; s < structType->structType.fieldCount; s++) {
                    if (strcmp(structType->structType.fields[s].name, fName) == 0) {
                        fExpectedType = structType->structType.fields[s].type;
                        break;
                    }
                }

                TypedExpr* fVal = LowerExpr(compiler, expr->structValue.fields[i].value, scope, fExpectedType);
                if (fVal && fExpectedType && !TypesAreEqual(fVal->type, fExpectedType)) {
                    fprintf(stderr, "[%u:%u] Type Error: Field '%s' type mismatch (expected '%s', got '%s')\n",
                            expr->line, expr->column, fName, TypeToString(fExpectedType), TypeToString(fVal->type));
                    compiler->hadError = true;
                }

                fields[i] = (TypedFieldValue){ .name = fName, .value = fVal };
            }

            return CreateTypedStructInitExpr(structType, fields, count, expr->line, expr->column);
        }

        case AST_EXPR_CALL: {
            // Check if callee is a constrained generic function (e.g. addCustom)
            if (expr->call.callee->kind == AST_EXPR_IDENT) {
                const Symbol* sym = SymbolTableLookup(scope, expr->call.callee->identName);

                if (sym && sym->typeDecl && sym->typeDecl->typeAnnotation &&
                    sym->typeDecl->typeAnnotation->kind == AST_TYPE_CONSTRAINED) {
                    
                    TypedExpr* loweredArg = LowerExpr(compiler, expr->call.argument, scope, nullptr);
                    if (!loweredArg) return nullptr;

                    const char* specName = MonomorphizeGenericFunction(compiler, sym, loweredArg->type, scope, expr->line, expr->column);
                    if (!specName) return nullptr;

                    const Symbol* specSym = SymbolTableLookup(compiler->globals, specName);
                    TypedExpr* specCallee = CreateTypedVarExpr(specSym->name, specSym->type, expr->line, expr->column);

                    return CreateTypedCallExpr(specCallee, loweredArg, specSym->type->function.returnType, expr->line, expr->column);
                }
            }

            TypedExpr* callee = LowerExpr(compiler, expr->call.callee, scope, nullptr);
            if (!callee || !callee->type || callee->type->kind != TYPE_FUNCTION) {
                fprintf(stderr, "[%u:%u] Type Error: Attempted to call non-function\n", expr->line, expr->column);
                compiler->hadError = true;
                return nullptr;
            }

            LanceType* paramType = callee->type->function.paramType;
            TypedExpr* arg = LowerExpr(compiler, expr->call.argument, scope, paramType);
            if (!arg) return nullptr;

            if (!TypesAreEqual(paramType, arg->type)) {
                fprintf(stderr, "[%u:%u] Type Error: Argument type mismatch (expected '%s', got '%s')\n",
                        expr->line, expr->column, TypeToString(paramType), TypeToString(arg->type));
                compiler->hadError = true;
                return nullptr;
            }

            return CreateTypedCallExpr(callee, arg, callee->type->function.returnType, expr->line, expr->column);
        }

        case AST_EXPR_COMPTIME:
            return LowerExpr(compiler, expr->comptime.inner, scope, expectedType);

        default:
            return nullptr;
    }
}

TypedModule* CompileModule(Compiler* compiler, const AstModule* astModule) {
    if (!astModule) return nullptr;

    compiler->typedModule = (TypedModule*)calloc(1, sizeof(TypedModule));

    // Pass 1: Register top-level type annotations for type-level bindings (type and type functions)
    for (size_t i = 0; i < astModule->count; i++) {
        const AstDecl* decl = &astModule->declarations[i];

        if (decl->kind == AST_DECL_TYPE_ANNOTATION && decl->paramCount == 0) {
            if (AstTypeIsType(decl->typeAnnotation)) {
                LanceType* type = ResolveAstType(compiler, decl->typeAnnotation, compiler->globals);
                SymbolTableInsert(compiler->globals, decl->name, type, (AstDecl*)decl, true);
            }
        }
    }

    // Pass 2: Register instances and evaluate type aliases (e.g. Vec2 = Vec f32)
    for (size_t i = 0; i < astModule->count; i++) {
        const AstDecl* decl = &astModule->declarations[i];

        if (decl->kind == AST_DECL_BINDING && decl->paramCount == 1) {
            const char* ifaceName = decl->name;
            const char* targetTypeName = decl->params[0];

            LanceType* targetType = GetPrimitiveTypeByName(targetTypeName);
            if (!targetType) {
                const Symbol* sym = SymbolTableLookup(compiler->globals, targetTypeName);
                if (sym) targetType = sym->type;
            }

            if (targetType) {
                RegisterInstance(&compiler->instances, ifaceName, targetType, (AstDecl*)decl);
            }
        }

        if (decl->kind == AST_DECL_BINDING) {
            Symbol* symbol = SymbolTableLookupCurrentScope(compiler->globals, decl->name);

            if (symbol && symbol->isTypeFunction) {
                symbol->valueDecl = (AstDecl*)decl;

                if (symbol->type == GetTypeType() && decl->paramCount == 0) {
                    LanceType* evaluatedType = EvalTypeExpr(compiler, decl->body, compiler->globals);
                    if (evaluatedType) {
                        if (evaluatedType->kind == TYPE_STRUCT) {
                            evaluatedType->structType.name = decl->name;
                        }
                        symbol->type = evaluatedType;
                    }
                }
            }
        }
    }

    // Pass 3: Register all value type annotations now that user types (e.g. Vec2) are resolved
    for (size_t i = 0; i < astModule->count; i++) {
        const AstDecl* decl = &astModule->declarations[i];

        if (decl->kind == AST_DECL_TYPE_ANNOTATION && decl->paramCount == 0) {
            Symbol* existing = SymbolTableLookupCurrentScope(compiler->globals, decl->name);
            if (!existing) {
                LanceType* type = ResolveAstType(compiler, decl->typeAnnotation, compiler->globals);
                SymbolTableInsert(compiler->globals, decl->name, type, (AstDecl*)decl, false);
            }
        }
    }

    // Pass 4: Lower executable value declarations into TypedDecl nodes in TypedModule
    for (size_t i = 0; i < astModule->count; i++) {
        const AstDecl* decl = &astModule->declarations[i];

        if (decl->kind == AST_DECL_BINDING) {
            // Skip instance implementations (paramCount == 1)
            if (decl->paramCount == 1) {
                const char* targetTypeName = decl->params[0];
                const LanceType* targetType = GetPrimitiveTypeByName(targetTypeName);
                if (!targetType) {
                    const Symbol* targetSym = SymbolTableLookup(compiler->globals, targetTypeName);
                    if (targetSym) targetType = targetSym->type;
                }
                if (targetType && LookupInstance(&compiler->instances, decl->name, targetType)) {
                    continue;
                }
            }

            Symbol* symbol = SymbolTableLookupCurrentScope(compiler->globals, decl->name);
            if (!symbol) {
                fprintf(stderr, "[%u:%u] Type Error: Binding '%s' missing explicit type annotation\n",
                        decl->line, decl->column, decl->name);
                compiler->hadError = true;
                continue;
            }

            // Skip type functions (they are compile-time only)
            if (symbol->isTypeFunction) {
                continue;
            }

            // Skip unspecialized constrained generic templates (specialized on demand)
            if (symbol->typeDecl && symbol->typeDecl->typeAnnotation &&
                symbol->typeDecl->typeAnnotation->kind == AST_TYPE_CONSTRAINED) {
                symbol->valueDecl = (AstDecl*)decl;
                continue;
            }

            symbol->valueDecl = (AstDecl*)decl;
            LanceType* declType = symbol->type;

            SymbolTable* localScope = CreateSymbolTable(compiler->globals);
            LanceType* currentType = declType;

            for (size_t p = 0; p < decl->paramCount; p++) {
                if (currentType && currentType->kind == TYPE_FUNCTION) {
                    SymbolTableInsert(localScope, decl->params[p], currentType->function.paramType, nullptr, false);
                    currentType = currentType->function.returnType;
                }
            }

            TypedExpr* loweredBody = LowerExpr(compiler, decl->body, localScope, currentType);

            if (loweredBody && currentType && !TypesAreEqual(loweredBody->type, currentType)) {
                fprintf(stderr, "[%u:%u] Type Error: In binding '%s', expected '%s' but got '%s'\n",
                        decl->line, decl->column, decl->name, TypeToString(currentType), TypeToString(loweredBody->type));
                compiler->hadError = true;
            }

            FreeSymbolTable(localScope);

            const char** paramsCopy = nullptr;
            if (decl->paramCount > 0) {
                paramsCopy = ALLOCATE(const char*, decl->paramCount);
                for (size_t p = 0; p < decl->paramCount; p++) {
                    paramsCopy[p] = decl->params[p];
                }
            }

            TypedDecl typedDecl = {
                .name = decl->name,
                .type = declType,
                .params = paramsCopy,
                .paramCount = decl->paramCount,
                .body = loweredBody,
                .line = decl->line,
                .column = decl->column
            };

            AppendTypedDecl(compiler->typedModule, typedDecl);
        }
    }

    if (compiler->hadError) {
        FreeTypedModule(compiler->typedModule);
        compiler->typedModule = nullptr;
        return nullptr;
    }

    return compiler->typedModule;
}
