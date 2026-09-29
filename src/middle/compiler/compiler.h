#ifndef LANCE_COMPILER_H
#define LANCE_COMPILER_H

#include <stdbool.h>

#include "arena.h"
#include "ast.h"
#include "symbol.h"
#include "type.h"
#include "typed_ast.h"
#include "vec.h"

// A source module being compiled. Its scope holds the module's top-level
// declarations; its parent is the builtin scope and its imports are the
// scopes of the modules it imports.
typedef struct {
    const AstModule* ast;
    SymbolTable* scope;
} CompilerModule;

// A struct-valued type function such as `Numeric T = { (+) :: T -> T -> T }`.
// Interfaces are the ones that have instances; generic structs like `Vec T`
// share the same shape.
typedef struct {
    const Symbol* symbol;         // The type function's symbol; identifies the interface
    const char* typeParam;        // e.g. "T"
    const AstType* methods;       // AST_TYPE_STRUCT listing the method signatures
    const CompilerModule* module; // Declaring module, where the signatures are resolved
} CompilerInterface;

// An instance such as `Numeric i32 = .{ (+) = +# , ... }`. Instances are
// global: once declared, they apply wherever their interface is visible.
typedef struct {
    const Symbol* interface;
    const LanceType* targetType;
    const AstDecl* instanceDecl;
    const CompilerModule* module; // Declaring module, where the methods are lowered
} CompilerInstance;

typedef VEC(CompilerInterface) CompilerInterfaceList;
typedef VEC(CompilerInstance) CompilerInstanceList;

typedef struct {
    SymbolTable* builtins; // Primitive operators and native functions
    CompilerModule* modules;
    size_t moduleCount;

    // What is being compiled right now.
    const CompilerModule* module; // Module of the current declaration
    SymbolTable* globals;         // Its scope: every top-level name visible there
    size_t frameSize;             // Call-frame slots used so far by the current binding

    Arena types;           // Owns every compound LanceType created during compilation
    Arena specializations; // Owns specialized AST expression copies
    CompilerInterfaceList interfaces;
    CompilerInstanceList instances;
    TypedModule* typedModule;
    bool hadError;
} Compiler;

void InitializeCompiler(Compiler* compiler);

// Releases the compiler, including every type it created. Typed modules
// returned by CompileProgram refer to these types, so free them first.
void FreeCompiler(Compiler* compiler);

// Compiles the modules of a program (the entry module first) into one fully
// typed, specialized TypedModule whose variables are all resolved to slots.
// Returns nullptr after reporting errors.
TypedModule* CompileProgram(Compiler* compiler, AstModule* const* modules, size_t moduleCount);

#endif // LANCE_COMPILER_H
