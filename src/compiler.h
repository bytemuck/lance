#ifndef LANCE_COMPILER_H
#define LANCE_COMPILER_H

#include <stdbool.h>

#include "ast.h"
#include "symbol.h"
#include "type.h"
#include "typed_ast.h"

typedef struct {
    const char* interfaceName;
    const LanceType* targetType;
    AstDecl* instanceDecl;
} CompilerInstanceEntry;

typedef struct {
    CompilerInstanceEntry* entries;
    size_t count;
    size_t capacity;
} CompilerInstanceRegistry;

typedef struct {
    LanceType** types;
    size_t count;
    size_t capacity;
} CompilerTypeRegistry;

typedef struct {
    SymbolTable* globals;
    CompilerInstanceRegistry instances;
    CompilerTypeRegistry allocatedTypes;
    TypedModule* typedModule;
    bool hadError;
} Compiler;

void InitializeCompiler(Compiler* compiler);
void FreeCompiler(Compiler* compiler);

// Compiles an AstModule into a fully typed, specialized TypedModule
TypedModule* CompileModule(Compiler* compiler, const AstModule* astModule);

#endif // LANCE_COMPILER_H
