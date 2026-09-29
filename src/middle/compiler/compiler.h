#ifndef LANCE_COMPILER_H
#define LANCE_COMPILER_H

#include <stdbool.h>

#include "arena.h"
#include "ast.h"
#include "symbol.h"
#include "type.h"
#include "typed_ast.h"
#include "vec.h"

// A struct-valued type function such as `Numeric T = { (+) :: T -> T -> T }`.
// Interfaces are the ones that have instances; generic structs like `Vec T`
// share the same shape.
typedef struct {
    const char* name;
    const char* typeParam;  // e.g. "T"
    const AstType* methods; // AST_TYPE_STRUCT listing the method signatures
} CompilerInterface;

// An instance such as `Numeric i32 = .{ (+) = +# , ... }`.
typedef struct {
    const char* interfaceName;
    const LanceType* targetType;
    const AstDecl* instanceDecl;
} CompilerInstance;

typedef VEC(CompilerInterface) CompilerInterfaceList;
typedef VEC(CompilerInstance) CompilerInstanceList;

typedef struct {
    SymbolTable* globals;
    Arena types; // Owns every compound LanceType created during compilation
    CompilerInterfaceList interfaces;
    CompilerInstanceList instances;
    TypedModule* typedModule;
    const char* currentFile; // File of the declaration being compiled, for diagnostics
    bool hadError;
} Compiler;

void InitializeCompiler(Compiler* compiler);

// Releases the compiler, including every type it created. Typed modules
// returned by CompileModule refer to these types, so free them first.
void FreeCompiler(Compiler* compiler);

// Compiles an AstModule into a fully typed, specialized TypedModule
TypedModule* CompileModule(Compiler* compiler, const AstModule* astModule);

#endif // LANCE_COMPILER_H
