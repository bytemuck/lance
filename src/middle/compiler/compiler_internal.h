#ifndef LANCE_COMPILER_INTERNAL_H
#define LANCE_COMPILER_INTERNAL_H

#include "compiler.h"
#include "primitives.h"

[[gnu::format(printf, 4, 5)]]
void CompilerError(Compiler *compiler, uint32_t line, uint32_t column, const char *format, ...);

void				  EnterModule(Compiler *compiler, const CompilerModule *module);
const CompilerModule *ModuleOfDecl(const Compiler *compiler, const AstDecl *decl);

LanceType *NewFunctionType(Compiler *compiler, LanceType *paramType, LanceType *returnType);
LanceType *NewBinaryOperatorType(Compiler *compiler, LanceType *valueType, LanceType *resultType);
LanceType *LookupTypeName(const char *name, const SymbolTable *scope);
LanceType *ResolveAstType(Compiler *compiler, const AstType *astType, const SymbolTable *scope);
LanceType *InstantiateGenericType(Compiler *compiler, const AstType *astType, const char *paramName,
								  const LanceType *concreteType, const SymbolTable *scope);
LanceType *EvalTypeExpr(Compiler *compiler, const AstExpr *expr, const SymbolTable *scope);
bool	   AstTypeIsType(const AstType *astType);

void RegisterInterface(Compiler *compiler, const AstDecl *definition);
void RegisterInstance(Compiler *compiler, const Symbol *interface, const LanceType *type, const AstDecl *decl);
const AstDecl *LookupInstance(const Compiler *compiler, const char *interfaceName, const LanceType *type);
const AstExpr *FindInstanceMethod(const AstDecl *instanceDecl, const char *methodName);

typedef struct {
	const AstDecl		 *instanceDecl;
	const AstExpr		 *methodExpr;
	const CompilerModule *module;	  // Where the instance is declared
	LanceType			 *methodType; // Interface signature instantiated for the target type
} InstanceMethodLookup;

InstanceMethodLookup LookupInstanceMethodForType(Compiler *compiler, const LanceType *targetType,
												 const char *methodName);
bool				 HasInstanceMethod(const Compiler *compiler, const char *methodName);

const Symbol *SpecializeGenericFunction(Compiler *compiler, const Symbol *symbol, TypedExpr **loweredArgs,
										size_t argCount, uint32_t line, uint32_t column);
void		  AppendTypedDecl(TypedModule *module, TypedDecl decl);

TypedExpr *LowerExpr(Compiler *compiler, const AstExpr *expr, const SymbolTable *scope, LanceType *expectedType);
bool	   LowerBinding(Compiler *compiler, const char *name, const char *const *params, size_t paramCount,
						LanceType *signature, const AstExpr *body, uint32_t line, uint32_t column);

bool LowerGlobalValue(Compiler *compiler, Symbol *symbol);

const TypedDecl *FindTypedDecl(const TypedModule *module, const char *name);
bool			 ValidateInlineBody(Compiler *compiler, const char *displayName, const TypedDecl *decl);
// Replaces a fully applied call of `decl` by its body. Takes ownership of `args`.
TypedExpr *ExpandInlineCall(Compiler *compiler, const TypedDecl *decl, TypedExpr **args, uint32_t line,
							uint32_t column);

void ResolveGlobalSlots(Compiler *compiler, TypedModule *module);

#endif // LANCE_COMPILER_INTERNAL_H
