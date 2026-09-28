//
// Created by amelia on 9/27/26.
//

#ifndef LANCE_SYMBOL_H
#define LANCE_SYMBOL_H

#include "ast.h"
#include "table.h"
#include "type.h"

typedef struct Symbol {
    const char* name;
    LanceType* type;
    AstDecl* typeDecl;
    AstDecl* valueDecl;
    bool isTypeFunction;
} Symbol;

typedef struct SymbolTable {
    struct SymbolTable* parent;
    Table table;
} SymbolTable;

SymbolTable* CreateSymbolTable(SymbolTable* parent);
void FreeSymbolTable(SymbolTable* table);

bool SymbolTableInsert(SymbolTable* table, const char* name, LanceType* type, AstDecl* decl, bool isTypeFunction);
Symbol* SymbolTableLookup(const SymbolTable* table, const char* name);
Symbol* SymbolTableLookupCurrentScope(const SymbolTable* table, const char* name);

#endif // LANCE_SYMBOL_H
