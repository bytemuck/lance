#include "compiler_internal.h"
#include "diag.h"
#include "memory.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void CompilerError(Compiler* compiler, const uint32_t line, const uint32_t column, const char* format, ...) {
    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    const char* file = compiler->module ? compiler->module->ast->file : nullptr;
    ReportError("Type Error", SOURCE_LOC(file, line, column), "%s", message);
    compiler->hadError = true;
}

void InitializeCompiler(Compiler* compiler) {
    *compiler = (Compiler){0};
    compiler->builtins = CreateSymbolTable(nullptr);
    compiler->globals = compiler->builtins;
    InitArena(&compiler->types);
    InitArena(&compiler->specializations);

    // Primitive operators are polymorphic CPU operations. Their concrete
    // function type is supplied by the surrounding instance or call.
    for (size_t i = 0; i < PRIMITIVE_OP_COUNT; i++) {
        SymbolTableInsert(compiler->builtins, kPrimitiveOperators[i].name, SYMBOL_BUILTIN, nullptr, nullptr);
    }
    SymbolTableInsert(compiler->builtins, LANCE_PRINT_NAME, SYMBOL_BUILTIN, nullptr, nullptr);
    SymbolTableInsert(compiler->builtins, LANCE_IF_NAME, SYMBOL_BUILTIN, nullptr, nullptr);
}

void FreeCompiler(Compiler* compiler) {
    for (size_t i = 0; i < compiler->moduleCount; i++) {
        FreeSymbolTable(compiler->modules[i].scope);
    }
    FREE_ARRAY(CompilerModule, compiler->modules, compiler->moduleCount);
    compiler->modules = nullptr;
    compiler->moduleCount = 0;
    FreeSymbolTable(compiler->builtins);
    compiler->builtins = nullptr;
    compiler->globals = nullptr;
    compiler->module = nullptr;
    VEC_FREE(compiler->interfaces);
    VEC_FREE(compiler->instances);
    FreeArena(&compiler->types);
    FreeArena(&compiler->specializations);
}

// ---- Modules -----------------------------------------------------------------

void EnterModule(Compiler* compiler, const CompilerModule* module) {
    compiler->module = module;
    compiler->globals = module ? module->scope : compiler->builtins;
}

const CompilerModule* ModuleOfDecl(const Compiler* compiler, const AstDecl* decl) {
    for (size_t i = 0; i < compiler->moduleCount; i++) {
        const AstModule* ast = compiler->modules[i].ast;
        if (decl >= ast->declarations && decl < ast->declarations + ast->count) return &compiler->modules[i];
    }
    return nullptr;
}

static const CompilerModule* FindCompilerModule(const Compiler* compiler, const AstModule* ast) {
    for (size_t i = 0; i < compiler->moduleCount; i++) {
        if (compiler->modules[i].ast == ast) return &compiler->modules[i];
    }
    return nullptr;
}

// One scope per module, linked to the scopes of the modules it imports.
static void CreateModuleScopes(Compiler* compiler, AstModule* const* modules, const size_t moduleCount) {
    compiler->moduleCount = moduleCount;
    compiler->modules = ALLOCATE(CompilerModule, moduleCount);
    for (size_t i = 0; i < moduleCount; i++) {
        SymbolTable* scope = CreateSymbolTable(compiler->builtins);
        scope->moduleName = modules[i]->name;
        compiler->modules[i] = (CompilerModule){ .ast = modules[i], .scope = scope };
    }

    for (size_t i = 0; i < moduleCount; i++) {
        const AstModule* ast = modules[i];
        for (size_t j = 0; j < ast->importCount; j++) {
            const CompilerModule* target = FindCompilerModule(compiler, ast->imports[j].module);
            if (target) SymbolTableAddImport(compiler->modules[i].scope, ast->imports[j].alias, target->scope);
        }
    }
}

// ---- Declaration roles -------------------------------------------------------

// What a top-level declaration means. Classified once, so the passes below
// don't each have to re-derive it from the declaration's shape.
typedef enum {
    DECL_ROLE_IGNORED,            // `import "..."`, or a declaration that failed to parse
    DECL_ROLE_TYPE_SIGNATURE,     // `Vec2 :: type`, `Numeric :: type -> type`
    DECL_ROLE_TYPE_DEFINITION,    // `Vec2 = Vec f32`, `Numeric T = { ... }`
    DECL_ROLE_INSTANCE_SIGNATURE, // `Numeric i32 :: type`
    DECL_ROLE_INSTANCE,           // `Numeric i32 = .{ ... }`
    DECL_ROLE_VALUE_SIGNATURE,    // `main :: i32`, `square :: (Numeric T) => T -> T`
    DECL_ROLE_VALUE,              // `main = ...`, `square x = ...`
} DeclRole;

static bool IsTypeFunctionName(const Compiler* compiler, const char* name) {
    const Symbol* symbol = SymbolTableLookup(compiler->globals, name);
    return symbol && symbol->kind == SYMBOL_TYPE_FUNCTION;
}

// `Interface Type` names an instance when the head is a type function and the
// argument is a known type (as opposed to a type parameter like `T`).
static bool IsInstanceHead(const Compiler* compiler, const AstDecl* decl) {
    return decl->paramCount == 1 && IsTypeFunctionName(compiler, decl->name) &&
           LookupTypeName(decl->params[0], compiler->globals) != nullptr;
}

static DeclRole ClassifyDecl(const Compiler* compiler, const AstDecl* decl) {
    if (!decl->name) return DECL_ROLE_IGNORED;

    switch (decl->kind) {
        case AST_DECL_IMPORT:
            return DECL_ROLE_IGNORED;

        case AST_DECL_TYPE_ANNOTATION:
            if (!decl->typeAnnotation) return DECL_ROLE_IGNORED;
            if (decl->paramCount == 0) {
                return AstTypeIsType(decl->typeAnnotation) ? DECL_ROLE_TYPE_SIGNATURE : DECL_ROLE_VALUE_SIGNATURE;
            }
            return IsInstanceHead(compiler, decl) ? DECL_ROLE_INSTANCE_SIGNATURE : DECL_ROLE_VALUE_SIGNATURE;

        case AST_DECL_BINDING: {
            if (!decl->body) return DECL_ROLE_IGNORED;
            if (IsInstanceHead(compiler, decl)) return DECL_ROLE_INSTANCE;

            const Symbol* symbol = SymbolTableLookupCurrentScope(compiler->globals, decl->name);
            return IsTypeLevelSymbol(symbol) ? DECL_ROLE_TYPE_DEFINITION : DECL_ROLE_VALUE;
        }
    }

    return DECL_ROLE_IGNORED;
}

// ---- Passes ------------------------------------------------------------------

typedef struct {
    const AstDecl* decl;
    DeclRole role;
    const CompilerModule* module;
} ClassifiedDecl;

typedef VEC(ClassifiedDecl) ClassifiedDeclList;

// Pass 1: declare every type-level name, so later passes can tell types from values.
static void CollectTypeSignatures(Compiler* compiler) {
    for (size_t m = 0; m < compiler->moduleCount; m++) {
        EnterModule(compiler, &compiler->modules[m]);
        const AstModule* module = compiler->modules[m].ast;

        for (size_t i = 0; i < module->count; i++) {
            const AstDecl* decl = &module->declarations[i];
            if (decl->kind != AST_DECL_TYPE_ANNOTATION || decl->paramCount != 0 || !decl->name ||
                !AstTypeIsType(decl->typeAnnotation)) {
                continue;
            }

            const SymbolKind kind = decl->typeAnnotation->kind == AST_TYPE_FUNCTION ? SYMBOL_TYPE_FUNCTION : SYMBOL_TYPE;
            LanceType* type = ResolveAstType(compiler, decl->typeAnnotation, compiler->globals);
            SymbolTableInsert(compiler->globals, decl->name, kind, type, (AstDecl*)decl);
        }
    }
}

static ClassifiedDeclList ClassifyDecls(Compiler* compiler) {
    ClassifiedDeclList decls = {0};
    for (size_t m = 0; m < compiler->moduleCount; m++) {
        const CompilerModule* module = &compiler->modules[m];
        EnterModule(compiler, module);
        for (size_t i = 0; i < module->ast->count; i++) {
            const AstDecl* decl = &module->ast->declarations[i];
            VEC_PUSH(decls, ((ClassifiedDecl){ .decl = decl, .role = ClassifyDecl(compiler, decl), .module = module }));
        }
    }
    return decls;
}

// Pass 2: attach type function bodies (`Vec T = {...}`), register interfaces,
// then evaluate type aliases (`Vec2 = Vec f32`) in source order.
static void DefineTypes(Compiler* compiler, const ClassifiedDeclList* decls) {
    for (size_t i = 0; i < decls->count; i++) {
        if (decls->items[i].role != DECL_ROLE_TYPE_DEFINITION) continue;
        const AstDecl* decl = decls->items[i].decl;
        EnterModule(compiler, decls->items[i].module);

        Symbol* symbol = SymbolTableLookupCurrentScope(compiler->globals, decl->name);
        if (symbol->kind == SYMBOL_TYPE_FUNCTION && decl->body->kind == AST_EXPR_TYPE) {
            symbol->valueDecl = (AstDecl*)decl;
            RegisterInterface(compiler, decl);
        }
    }

    for (size_t i = 0; i < decls->count; i++) {
        if (decls->items[i].role != DECL_ROLE_TYPE_DEFINITION) continue;
        const AstDecl* decl = decls->items[i].decl;
        EnterModule(compiler, decls->items[i].module);

        Symbol* symbol = SymbolTableLookupCurrentScope(compiler->globals, decl->name);
        if (symbol->kind != SYMBOL_TYPE || decl->paramCount != 0) continue;

        symbol->valueDecl = (AstDecl*)decl;
        LanceType* evaluated = EvalTypeExpr(compiler, decl->body, compiler->globals);
        if (!evaluated) {
            CompilerError(compiler, decl->line, decl->column, "Could not evaluate type '%s'", decl->name);
            continue;
        }
        if (evaluated->kind == TYPE_STRUCT && !evaluated->structType.name) {
            evaluated->structType.name = decl->name;
        }
        symbol->type = evaluated;
    }
}

// Pass 3: register interface instances now that their target types are known.
static void CollectInstances(Compiler* compiler, const ClassifiedDeclList* decls) {
    for (size_t i = 0; i < decls->count; i++) {
        if (decls->items[i].role != DECL_ROLE_INSTANCE) continue;
        const AstDecl* decl = decls->items[i].decl;
        EnterModule(compiler, decls->items[i].module);

        const Symbol* interface = SymbolTableLookup(compiler->globals, decl->name);
        const LanceType* target = LookupTypeName(decl->params[0], compiler->globals);
        if (LookupInstance(compiler, decl->name, target)) {
            CompilerError(compiler, decl->line, decl->column, "Duplicate instance '%s %s'",
                          decl->name, decl->params[0]);
            continue;
        }
        RegisterInstance(compiler, interface, target, decl);
    }
}

typedef VEC(AstConstraint) ConstraintList;

static bool DeclaresTypeParam(const ConstraintList* params, const char* name) {
    for (size_t i = 0; i < params->count; i++) {
        if (strcmp(params->items[i].typeParam, name) == 0) return true;
    }
    return false;
}

// Adds every type name in `type` that is not declared in the current module
// as an implicit, unconstrained type parameter.
static void CollectImplicitTypeParams(const Compiler* compiler, const AstType* type, ConstraintList* params) {
    if (!type) return;

    switch (type->kind) {
        case AST_TYPE_NAMED:
            if (!LookupTypeName(type->named.name, compiler->globals) && !DeclaresTypeParam(params, type->named.name)) {
                VEC_PUSH(*params, ((AstConstraint){ .interfaceName = nullptr, .typeParam = type->named.name }));
            }
            return;

        case AST_TYPE_FUNCTION:
            CollectImplicitTypeParams(compiler, type->function.paramType, params);
            CollectImplicitTypeParams(compiler, type->function.returnType, params);
            return;

        case AST_TYPE_LAZY:
            CollectImplicitTypeParams(compiler, type->lazy.inner, params);
            return;

        case AST_TYPE_STRUCT:
            for (size_t i = 0; i < type->structType.fieldCount; i++) {
                CollectImplicitTypeParams(compiler, type->structType.fields[i].type, params);
            }
            return;

        case AST_TYPE_CONSTRAINED:
            return;
    }
}

// A function signature is generic if it has constraints, `(Numeric T) => ...`,
// or mentions undeclared type names, `bool -> lazy T -> lazy T -> T`. Returns
// the signature with every type parameter listed as a constraint, or null.
static const AstType* GenericSignature(Compiler* compiler, const AstType* annotation) {
    const bool constrained = annotation->kind == AST_TYPE_CONSTRAINED;
    const AstType* target = constrained ? annotation->constrained.targetType : annotation;
    if (!target || (!constrained && target->kind != AST_TYPE_FUNCTION)) return nullptr;

    ConstraintList params = {0};
    for (size_t i = 0; constrained && i < annotation->constrained.constraintCount; i++) {
        VEC_PUSH(params, annotation->constrained.constraints[i]);
    }
    const size_t explicitCount = params.count;
    CollectImplicitTypeParams(compiler, target, &params);

    const AstType* generic = annotation;
    if (params.count > explicitCount) {
        AstConstraint* constraints = ARENA_ARRAY(&compiler->specializations, AstConstraint, params.count);
        memcpy(constraints, params.items, params.count * sizeof(AstConstraint));
        generic = CreateConstrainedTypeAst(&compiler->specializations, constraints, params.count, (AstType*)target,
                                           annotation->line, annotation->column);
    } else if (!constrained) {
        generic = nullptr;
    }

    VEC_FREE(params);
    return generic;
}

// Pass 4: resolve value signatures, which may mention the types defined above.
static void CollectValueSignatures(Compiler* compiler, const ClassifiedDeclList* decls) {
    for (size_t i = 0; i < decls->count; i++) {
        if (decls->items[i].role != DECL_ROLE_VALUE_SIGNATURE) continue;
        const AstDecl* decl = decls->items[i].decl;
        EnterModule(compiler, decls->items[i].module);

        if (decl->paramCount != 0) {
            CompilerError(compiler, decl->line, decl->column,
                          "Type annotation for '%s' cannot have parameters", decl->name);
            continue;
        }
        if (SymbolTableLookupCurrentScope(compiler->globals, decl->name)) {
            CompilerError(compiler, decl->line, decl->column, "Duplicate declaration of '%s'", decl->name);
            continue;
        }

        const AstType* generic = GenericSignature(compiler, decl->typeAnnotation);
        LanceType* type = ResolveAstType(compiler, generic ? generic : decl->typeAnnotation, compiler->globals);
        SymbolTableInsert(compiler->globals, decl->name, SYMBOL_VALUE, type, (AstDecl*)decl);
        SymbolTableLookupCurrentScope(compiler->globals, decl->name)->generic = generic;
    }
}

// Pass 5a: attach value bodies, so generic templates are available before
// anything that calls them is lowered.
static void AttachValueBodies(Compiler* compiler, const ClassifiedDeclList* decls) {
    for (size_t i = 0; i < decls->count; i++) {
        if (decls->items[i].role != DECL_ROLE_VALUE) continue;
        const AstDecl* decl = decls->items[i].decl;
        EnterModule(compiler, decls->items[i].module);

        Symbol* symbol = SymbolTableLookupCurrentScope(compiler->globals, decl->name);
        if (!symbol || symbol->kind != SYMBOL_VALUE) {
            CompilerError(compiler, decl->line, decl->column,
                          "Binding '%s' missing explicit type annotation", decl->name);
            continue;
        }
        if (symbol->valueDecl) {
            CompilerError(compiler, decl->line, decl->column, "Duplicate definition of '%s'", decl->name);
            continue;
        }
        symbol->valueDecl = (AstDecl*)decl;
    }
}

// Pass 5b: lower every non-generic value. Generic templates are only lowered
// on demand, once per concrete type, by SpecializeGenericFunction.
static void LowerValues(Compiler* compiler, const ClassifiedDeclList* decls) {
    for (size_t i = 0; i < decls->count; i++) {
        if (decls->items[i].role != DECL_ROLE_VALUE) continue;
        const AstDecl* decl = decls->items[i].decl;
        EnterModule(compiler, decls->items[i].module);

        const Symbol* symbol = SymbolTableLookupCurrentScope(compiler->globals, decl->name);
        if (!symbol || symbol->valueDecl != decl || symbol->generic || !symbol->type) continue;

        LowerBinding(compiler, symbol->globalName, decl->params, decl->paramCount, symbol->type,
                     decl->body, decl->line, decl->column);
    }
}

TypedModule* CompileProgram(Compiler* compiler, AstModule* const* modules, const size_t moduleCount) {
    if (!modules || moduleCount == 0) return nullptr;

    compiler->typedModule = (TypedModule*)calloc(1, sizeof(TypedModule));
    CreateModuleScopes(compiler, modules, moduleCount);

    CollectTypeSignatures(compiler);
    ClassifiedDeclList decls = ClassifyDecls(compiler);
    DefineTypes(compiler, &decls);
    CollectInstances(compiler, &decls);
    CollectValueSignatures(compiler, &decls);
    AttachValueBodies(compiler, &decls);
    LowerValues(compiler, &decls);
    VEC_FREE(decls);

    EnterModule(compiler, nullptr);
    if (!compiler->hadError) ResolveGlobalSlots(compiler, compiler->typedModule);

    if (compiler->hadError) {
        FreeTypedModule(compiler->typedModule);
        compiler->typedModule = nullptr;
        return nullptr;
    }

    return compiler->typedModule;
}
