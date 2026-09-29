#ifndef LANCE_COMPILER_INTERNAL_H
#define LANCE_COMPILER_INTERNAL_H

// Shared declarations between the compiler's translation units:
//   compiler.c      declaration roles and the compilation passes
//   resolve_type.c  AST types -> LanceTypes, type-level evaluation
//   instances.c     interface and instance registries, method lookup
//   specialize.c    monomorphization of constrained generic functions
//   lower_expr.c    AST expressions -> typed expressions

#include "compiler.h"
#include "primitives.h"

// ---- Diagnostics -----------------------------------------------------------

[[gnu::format(printf, 4, 5)]]
void CompilerError(Compiler* compiler, uint32_t line, uint32_t column, const char* format, ...);

// ---- resolve_type.c --------------------------------------------------------

LanceType* NewFunctionType(Compiler* compiler, LanceType* paramType, LanceType* returnType);

// valueType -> valueType -> resultType
LanceType* NewBinaryOperatorType(Compiler* compiler, LanceType* valueType, LanceType* resultType);

// Resolves a type name to a primitive type or a type bound in `scope`.
LanceType* LookupTypeName(const char* name, const SymbolTable* scope);

LanceType* ResolveAstType(Compiler* compiler, const AstType* astType, const SymbolTable* scope);

// Resolves `astType` with `paramName` bound to `concreteType`.
LanceType* InstantiateGenericType(Compiler* compiler, const AstType* astType, const char* paramName,
                                  const LanceType* concreteType, const SymbolTable* scope);

// Evaluates a type-level expression such as `Vec f32`.
LanceType* EvalTypeExpr(Compiler* compiler, const AstExpr* expr, const SymbolTable* scope);

// True for `type` and type functions such as `type -> type`.
bool AstTypeIsType(const AstType* astType);

// ---- instances.c -----------------------------------------------------------

void RegisterInterface(Compiler* compiler, const AstDecl* definition);
const CompilerInterface* LookupInterface(const Compiler* compiler, const char* name);

void RegisterInstance(Compiler* compiler, const char* interfaceName, const LanceType* type, const AstDecl* decl);
const AstDecl* LookupInstance(const Compiler* compiler, const char* interfaceName, const LanceType* type);

// The expression bound to `methodName` in an instance body, if any.
const AstExpr* FindInstanceMethod(const AstDecl* instanceDecl, const char* methodName);

typedef struct {
    const AstDecl* instanceDecl;
    const AstExpr* methodExpr;
    LanceType* methodType; // Interface signature instantiated for the target type
} InstanceMethodLookup;

// Finds `methodName` in any instance for `targetType`, in whatever interface
// declares it, so no interface names need to be hard-coded.
InstanceMethodLookup LookupInstanceMethodForType(Compiler* compiler, const LanceType* targetType, const char* methodName);

// True if some instance defines `methodName`.
bool HasInstanceMethod(const Compiler* compiler, const char* methodName);

// ---- specialize.c ----------------------------------------------------------

// Specializes the constrained generic `symbol` for the argument types and
// returns the symbol of the specialization (e.g. `square$i32`).
const Symbol* SpecializeGenericFunction(Compiler* compiler, const Symbol* symbol, TypedExpr** loweredArgs,
                                        size_t argCount, const SymbolTable* scope, uint32_t line, uint32_t column);

void AppendTypedDecl(TypedModule* module, TypedDecl decl);

// ---- lower_expr.c ----------------------------------------------------------

TypedExpr* LowerExpr(Compiler* compiler, const AstExpr* expr, const SymbolTable* scope, LanceType* expectedType);

// Lowers the body of `name params... = body` against `signature` and appends
// the result to the typed module. Returns false on a type error.
bool LowerBinding(Compiler* compiler, const char* name, const char* const* params, size_t paramCount,
                  LanceType* signature, const AstExpr* body, uint32_t line, uint32_t column);

#endif // LANCE_COMPILER_INTERNAL_H
