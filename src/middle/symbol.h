//
// Created by amelia on 9/27/26.
//

#ifndef LANCE_SYMBOL_H
#define LANCE_SYMBOL_H

#include "ast.h"
#include "table.h"
#include "type.h"
#include "vec.h"

typedef enum {
    SYMBOL_VALUE,         // Global value or function: `main :: i32`
    SYMBOL_LOCAL,         // Parameter or `let` binding, stored in a call-frame slot
    SYMBOL_TYPE,          // Type or type alias: `Vec2 :: type`, a bound type parameter
    SYMBOL_TYPE_FUNCTION, // Type constructor or interface: `Vec :: type -> type`
    SYMBOL_BUILTIN,       // Primitive operator or native function provided by the runtime
} SymbolKind;

typedef struct Symbol {
    const char* name;       // As written in the source
    const char* globalName; // Unique in the program: `name`, or `module.name` in an imported module
    SymbolKind kind;
    LanceType* type;
    size_t slot;            // SYMBOL_LOCAL only: index in the call frame
    AstDecl* typeDecl;      // The `name :: T` annotation, if any
    AstDecl* valueDecl;     // The `name ... = body` binding, if any
    // SYMBOL_VALUE only: for a generic function, its signature as
    // `(Constraints) => T`, including implicit type parameters (constraints
    // without an interface). Null for ordinary values.
    const AstType* generic;
} Symbol;

typedef struct SymbolTable SymbolTable;

// `import "util.lance"` as seen from the importing module's scope.
typedef struct {
    const char* alias;
    SymbolTable* module;
} SymbolImport;

struct SymbolTable {
    SymbolTable* parent;
    Table table;            // Interned name -> Symbol*
    const char* moduleName; // Qualifies globals declared here; null for the entry module and local scopes
    VEC(SymbolImport) imports;
};

SymbolTable* CreateSymbolTable(SymbolTable* parent);
void FreeSymbolTable(SymbolTable* table);

bool SymbolTableInsert(SymbolTable* table, const char* name, SymbolKind kind, LanceType* type, AstDecl* decl);
Symbol* SymbolTableInsertLocal(SymbolTable* table, const char* name, LanceType* type, size_t slot);

// Looks `name` up in `table`, then in the modules it imports, then in its
// parent. A name imported from several modules is ambiguous and not found.
Symbol* SymbolTableLookup(const SymbolTable* table, const char* name);
Symbol* SymbolTableLookupCurrentScope(const SymbolTable* table, const char* name);
bool SymbolTableIsAmbiguous(const SymbolTable* table, const char* name);

// Returns false if `alias` already names a different module.
bool SymbolTableAddImport(SymbolTable* table, const char* alias, SymbolTable* module);
// The module imported as `alias`, unless a symbol of that name shadows it.
// Look qualified names up in it with SymbolTableLookupCurrentScope.
const SymbolTable* SymbolTableFindModule(const SymbolTable* table, const char* alias);

static inline bool IsTypeLevelSymbol(const Symbol* symbol) {
    return symbol && (symbol->kind == SYMBOL_TYPE || symbol->kind == SYMBOL_TYPE_FUNCTION);
}

#endif // LANCE_SYMBOL_H
