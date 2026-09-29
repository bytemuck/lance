#ifndef LANCE_COMPILER_INTERNAL_H
#define LANCE_COMPILER_INTERNAL_H

// Shared declarations between the compiler's translation units:
//   compiler.c      declaration roles and the compilation passes
//   resolve_type.c  AST types -> LanceTypes, type-level evaluation
//   instances.c     interface and instance registries, method lookup
//   specialize.c    monomorphization of constrained generic functions
//   lower_expr.c    AST expressions -> typed expressions
//   resolve_slots.c global variable references -> declaration indices

#include "compiler.h"
#include "primitives.h"

// ---- Diagnostics -----------------------------------------------------------

[[gnu::format(printf, 4, 5)]]
void CompilerError(Compiler* compiler, uint32_t line, uint32_t column, const char* format, ...);

// ---- Modules ---------------------------------------------------------------

// Makes `module` the current module: its scope becomes `compiler->globals`
// and its file is used for diagnostics.
void EnterModule(Compiler* compiler, const CompilerModule* module);

// The module whose source contains `decl`.
const CompilerModule* ModuleOfDecl(const Compiler* compiler, const AstDecl* decl);

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

// Both register into the current module.
void RegisterInterface(Compiler* compiler, const AstDecl* definition);
void RegisterInstance(Compiler* compiler, const Symbol* interface, const LanceType* type, const AstDecl* decl);

// Finds the instance of the interface called `interfaceName` in the current
// module for `type`.
const AstDecl* LookupInstance(const Compiler* compiler, const char* interfaceName, const LanceType* type);

// The expression bound to `methodName` in an instance body, if any.
const AstExpr* FindInstanceMethod(const AstDecl* instanceDecl, const char* methodName);

typedef struct {
    const AstDecl* instanceDecl;
    const AstExpr* methodExpr;
    const CompilerModule* module; // Where the instance, and so the method, is declared
    LanceType* methodType;        // Interface signature instantiated for the target type
} InstanceMethodLookup;

// Finds `methodName` in any instance for `targetType` whose interface is
// visible in the current module, so no interface names need to be hard-coded.
InstanceMethodLookup LookupInstanceMethodForType(Compiler* compiler, const LanceType* targetType, const char* methodName);

// True if some instance of an interface visible in the current module
// defines `methodName`.
bool HasInstanceMethod(const Compiler* compiler, const char* methodName);

// ---- specialize.c ----------------------------------------------------------

// Specializes the constrained generic `symbol` for the argument types and
// returns the symbol of the specialization (e.g. `square$i32`), which lives
// in the module that defines `symbol`.
const Symbol* SpecializeGenericFunction(Compiler* compiler, const Symbol* symbol, TypedExpr** loweredArgs,
                                        size_t argCount, uint32_t line, uint32_t column);

void AppendTypedDecl(TypedModule* module, TypedDecl decl);

// ---- lower_expr.c ----------------------------------------------------------

TypedExpr* LowerExpr(Compiler* compiler, const AstExpr* expr, const SymbolTable* scope, LanceType* expectedType);

// Lowers the body of `name params... = body` against `signature` and appends
// the result to the typed module. Returns false on a type error.
bool LowerBinding(Compiler* compiler, const char* name, const char* const* params, size_t paramCount,
                  LanceType* signature, const AstExpr* body, uint32_t line, uint32_t column);

// ---- resolve_slots.c -------------------------------------------------------

// Points every SLOT_GLOBAL reference at its declaration. Runs once, after all
// declarations and specializations have been lowered.
void ResolveGlobalSlots(Compiler* compiler, TypedModule* module);

#endif // LANCE_COMPILER_INTERNAL_H
