//
// Created by amelia on 9/27/26.
//

#include "symbol.h"
#include "memory.h"
#include "string_pool.h"

#include <stdlib.h>
#include <string.h>

SymbolTable* CreateSymbolTable(SymbolTable* parent) {
    SymbolTable* table = ALLOCATE(SymbolTable, 1);
    if (!table) {
        return nullptr;
    }

    table->parent = parent;
    TableInit(&table->table);
    return table;
}

void FreeSymbolTable(SymbolTable* table) {
    if (!table) {
        return;
    }

    for (size_t i = 0; i < table->table.capacity; i++) {
        TableEntry* entry = &table->table.entries[i];
        if (entry->key != nullptr && entry->value != nullptr && entry->value != (void*)1) {
            FREE(Symbol, (Symbol*)entry->value);
        }
    }

    TableFree(&table->table);
    FREE(SymbolTable, table);
}

Symbol* SymbolTableLookupCurrentScope(const SymbolTable* table, const char* name) {
    if (!table || !name) {
        return nullptr;
    }

    const char* interned = InternCString(name);
    return (Symbol*)TableGet(&table->table, interned);
}

Symbol* SymbolTableLookup(const SymbolTable* table, const char* name) {
    if (!name) {
        return nullptr;
    }

    const char* interned = InternCString(name);
    const SymbolTable* current = table;
    while (current != nullptr) {
        Symbol* symbol = (Symbol*)TableGet(&current->table, interned);
        if (symbol) {
            return symbol;
        }
        current = current->parent;
    }

    return nullptr;
}

bool SymbolTableInsert(SymbolTable* table, const char* name, const SymbolKind kind, LanceType* type, AstDecl* decl) {
    if (!table || !name) {
        return false;
    }

    const char* interned = InternCString(name);
    Symbol* existing = SymbolTableLookupCurrentScope(table, interned);
    if (existing) {
        if (decl && decl->kind == AST_DECL_TYPE_ANNOTATION) existing->typeDecl = decl;
        if (decl && decl->kind == AST_DECL_BINDING) existing->valueDecl = decl;
        if (type) existing->type = type;
        return true;
    }

    Symbol* sym = ALLOCATE(Symbol, 1);
    sym->name = interned;
    sym->type = type;
    sym->typeDecl = (decl && decl->kind == AST_DECL_TYPE_ANNOTATION) ? decl : nullptr;
    sym->valueDecl = (decl && decl->kind == AST_DECL_BINDING) ? decl : nullptr;
    sym->kind = kind;

    TableSet(&table->table, interned, sym);
    return true;
}