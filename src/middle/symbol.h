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
	SYMBOL_VALUE,		  // Global value or function: `main :: i32`
	SYMBOL_LOCAL,		  // Parameter or `let` binding, stored in a call-frame slot
	SYMBOL_TYPE,		  // Type or type alias: `Vec2 :: type`, a bound type parameter
	SYMBOL_TYPE_FUNCTION, // Type constructor or interface: `Vec :: type -> type`
	SYMBOL_BUILTIN,		  // Primitive operator or native function provided by the runtime
} SymbolKind;

typedef enum {
	LOWER_PENDING,	   // The body has not been lowered yet
	LOWER_IN_PROGRESS, // The body is being lowered right now
	LOWER_DONE,		   // The body was lowered, or failed to lower
} LowerState;

typedef struct Symbol {
	const char	  *name;
	const char	  *globalName; // Unique in the program: `name`, or `module.name` in an imported module
	SymbolKind	   kind;
	LanceType	  *type;
	size_t		   slot;	  // SYMBOL_LOCAL only: index in the call frame
	AstDecl		  *typeDecl;  // The `name :: T` annotation, if any
	AstDecl		  *valueDecl; // The `name ... = body` binding, if any
	const AstType *generic;	  // (Constraints) => T
	bool		   isInline;   // `... -> inline T`: fully applied calls are expanded in place
	LowerState	   lowerState; // Lets an inline expansion find its body, or detect recursion
} Symbol;

typedef struct SymbolTable SymbolTable;

typedef struct {
	const char	*alias;
	SymbolTable *module;
} SymbolImport;

struct SymbolTable {
	SymbolTable *parent;
	Table		 table;		 // Interned name -> Symbol*
	const char	*moduleName; // Qualifies globals declared here; null for the entry module and local scopes
	VEC(SymbolImport) imports;
};

SymbolTable *CreateSymbolTable(SymbolTable *parent);
void		 FreeSymbolTable(SymbolTable *table);

bool	SymbolTableInsert(SymbolTable *table, const char *name, SymbolKind kind, LanceType *type, AstDecl *decl);
Symbol *SymbolTableInsertLocal(SymbolTable *table, const char *name, LanceType *type, size_t slot);

Symbol *SymbolTableLookup(const SymbolTable *table, const char *name);
Symbol *SymbolTableLookupCurrentScope(const SymbolTable *table, const char *name);
bool	SymbolTableIsAmbiguous(const SymbolTable *table, const char *name);

bool			   SymbolTableAddImport(SymbolTable *table, const char *alias, SymbolTable *module);
const SymbolTable *SymbolTableFindModule(const SymbolTable *table, const char *alias);

static bool IsTypeLevelSymbol(const Symbol* symbol) {
	return symbol && (symbol->kind == SYMBOL_TYPE || symbol->kind == SYMBOL_TYPE_FUNCTION);
}

#endif // LANCE_SYMBOL_H
