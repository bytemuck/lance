//
// Created by amelia on 9/27/26.
//

#ifndef LANCE_SYMBOL_H
#define LANCE_SYMBOL_H

#include "ast.h"
#include "table.h"
#include "type.h"

typedef enum {
    SYMBOL_VALUE,         // Runtime value or function: `main :: i32`
    SYMBOL_TYPE,          // Type or type alias: `Vec2 :: type`, a bound type parameter
    SYMBOL_TYPE_FUNCTION, // Type constructor or interface: `Vec :: type -> type`
    SYMBOL_BUILTIN,       // Primitive operator or native function provided by the runtime
} SymbolKind;

typedef struct Symbol {
    const char* name;
    SymbolKind kind;
    LanceType* type;
    AstDecl* typeDecl;  // The `name :: T` annotation, if any
    AstDecl* valueDecl; // The `name ... = body` binding, if any
} Symbol;

typedef struct SymbolTable {
    struct SymbolTable* parent;
    Table table;
} SymbolTable;

SymbolTable* CreateSymbolTable(SymbolTable* parent);
void FreeSymbolTable(SymbolTable* table);

bool SymbolTableInsert(SymbolTable* table, const char* name, SymbolKind kind, LanceType* type, AstDecl* decl);
Symbol* SymbolTableLookup(const SymbolTable* table, const char* name);
Symbol* SymbolTableLookupCurrentScope(const SymbolTable* table, const char* name);

static inline bool IsTypeLevelSymbol(const Symbol* symbol) {
    return symbol && (symbol->kind == SYMBOL_TYPE || symbol->kind == SYMBOL_TYPE_FUNCTION);
}

#endif // LANCE_SYMBOL_H
