//
// Created by amelia on 9/27/26.
//

#include "symbol.h"
#include "memory.h"
#include "string_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SymbolTable* CreateSymbolTable(SymbolTable* parent) {
    SymbolTable* table = ALLOCATE(SymbolTable, 1);
    *table = (SymbolTable){ .parent = parent };
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
    VEC_FREE(table->imports);
    FREE(SymbolTable, table);
}

Symbol* SymbolTableLookupCurrentScope(const SymbolTable* table, const char* name) {
    if (!table || !name) {
        return nullptr;
    }

    return (Symbol*)TableGet(&table->table, InternCString(name));
}

// Finds `name` among the declarations of the modules `table` imports directly.
static Symbol* LookupImported(const SymbolTable* table, const char* name, bool* ambiguous) {
    Symbol* found = nullptr;
    for (size_t i = 0; i < table->imports.count; i++) {
        Symbol* candidate = SymbolTableLookupCurrentScope(table->imports.items[i].module, name);
        if (!candidate) continue;
        if (found && found != candidate) {
            *ambiguous = true;
            return nullptr;
        }
        found = candidate;
    }
    return found;
}

static Symbol* Lookup(const SymbolTable* table, const char* name, bool* ambiguous) {
    *ambiguous = false;
    if (!name) {
        return nullptr;
    }

    const char* interned = InternCString(name);
    for (const SymbolTable* current = table; current; current = current->parent) {
        Symbol* symbol = (Symbol*)TableGet(&current->table, interned);
        if (symbol) return symbol;

        symbol = LookupImported(current, interned, ambiguous);
        if (symbol || *ambiguous) return symbol;
    }
    return nullptr;
}

Symbol* SymbolTableLookup(const SymbolTable* table, const char* name) {
    bool ambiguous;
    return Lookup(table, name, &ambiguous);
}

bool SymbolTableIsAmbiguous(const SymbolTable* table, const char* name) {
    bool ambiguous;
    Lookup(table, name, &ambiguous);
    return ambiguous;
}

bool SymbolTableAddImport(SymbolTable* table, const char* alias, SymbolTable* module) {
    const char* interned = InternCString(alias);
    for (size_t i = 0; i < table->imports.count; i++) {
        if (table->imports.items[i].alias == interned) {
            return table->imports.items[i].module == module;
        }
    }
    VEC_PUSH(table->imports, ((SymbolImport){ .alias = interned, .module = module }));
    return true;
}

const SymbolTable* SymbolTableFindModule(const SymbolTable* table, const char* alias) {
    const char* interned = InternCString(alias);
    for (const SymbolTable* current = table; current; current = current->parent) {
        if (TableGet(&current->table, interned)) return nullptr;
        for (size_t i = 0; i < current->imports.count; i++) {
            if (current->imports.items[i].alias == interned) return current->imports.items[i].module;
        }
    }
    return nullptr;
}

// `module.name` for globals of imported modules, `name` everywhere else.
static const char* GlobalName(const SymbolTable* table, const char* interned, const SymbolKind kind) {
    if (!table->moduleName || kind == SYMBOL_LOCAL || kind == SYMBOL_BUILTIN) {
        return interned;
    }

    const size_t length = strlen(table->moduleName) + 1 + strlen(interned) + 1;
    char* buffer = ALLOCATE(char, length);
    snprintf(buffer, length, "%s.%s", table->moduleName, interned);
    const char* qualified = InternCString(buffer);
    FREE_ARRAY(char, buffer, length);
    return qualified;
}

static Symbol* NewSymbol(SymbolTable* table, const char* interned, const SymbolKind kind, LanceType* type) {
    Symbol* symbol = ALLOCATE(Symbol, 1);
    *symbol = (Symbol){
        .name = interned,
        .globalName = GlobalName(table, interned, kind),
        .kind = kind,
        .type = type,
    };
    TableSet(&table->table, interned, symbol);
    return symbol;
}

bool SymbolTableInsert(SymbolTable* table, const char* name, const SymbolKind kind, LanceType* type, AstDecl* decl) {
    if (!table || !name) {
        return false;
    }

    const char* interned = InternCString(name);
    Symbol* symbol = SymbolTableLookupCurrentScope(table, interned);
    if (!symbol) {
        symbol = NewSymbol(table, interned, kind, type);
    } else if (type) {
        symbol->type = type;
    }

    if (decl && decl->kind == AST_DECL_TYPE_ANNOTATION) symbol->typeDecl = decl;
    if (decl && decl->kind == AST_DECL_BINDING) symbol->valueDecl = decl;
    return true;
}

Symbol* SymbolTableInsertLocal(SymbolTable* table, const char* name, LanceType* type, const size_t slot) {
    if (!table || !name) {
        return nullptr;
    }

    const char* interned = InternCString(name);
    Symbol* symbol = SymbolTableLookupCurrentScope(table, interned);
    if (!symbol) symbol = NewSymbol(table, interned, SYMBOL_LOCAL, type);
    symbol->kind = SYMBOL_LOCAL;
    symbol->type = type;
    symbol->slot = slot;
    return symbol;
}
