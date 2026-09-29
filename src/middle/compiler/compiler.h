#ifndef LANCE_COMPILER_H
#define LANCE_COMPILER_H

#include "arena.h"
#include "ast.h"
#include "symbol.h"
#include "type.h"
#include "typed_ast.h"
#include "vec.h"

typedef struct {
	const AstModule *ast;
	SymbolTable		*scope;
} CompilerModule;

typedef struct {
	const Symbol		 *symbol;
	const char			 *typeParam; // e.g. "T"
	const AstType		 *methods;	 // AST_TYPE_STRUCT listing the method signatures
	const CompilerModule *module;	 // Declaring module, where the signatures are resolved
} CompilerInterface;

typedef struct {
	const Symbol		 *interface;
	const LanceType		 *targetType;
	const AstDecl		 *instanceDecl;
	const CompilerModule *module; // Declaring module, where the methods are lowered
} CompilerInstance;

typedef VEC(CompilerInterface) CompilerInterfaceList;
typedef VEC(CompilerInstance) CompilerInstanceList;

typedef struct {
	SymbolTable	   *builtins; // Primitive operators and native functions
	CompilerModule *modules;
	size_t			moduleCount;

	const CompilerModule *module;	 // Module of the current declaration
	SymbolTable			 *globals;	 // Every top-level name visible from the module
	size_t				  frameSize; // Call-frame slots used so far by the current binding

	Arena				  types;		   // Owns every compound LanceType created during compilation
	Arena				  specializations; // Owns specialized AST expression
	CompilerInterfaceList interfaces;
	CompilerInstanceList  instances;
	TypedModule			 *typedModule;
	bool				  hadError;
} Compiler;

void InitializeCompiler(Compiler *compiler);
void FreeCompiler(Compiler *compiler);

TypedModule *CompileProgram(Compiler *compiler, AstModule *const *modules, size_t moduleCount);

#endif // LANCE_COMPILER_H
