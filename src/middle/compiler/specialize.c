#include "compiler_internal.h"
#include "string_pool.h"
#include "table.h"

#include <string.h>

// Binds a constrained type parameter (e.g. `T`) to the instance chosen for it.
// Unconstrained type parameters have no instance.
typedef struct {
	const char	  *typeParam;
	const AstDecl *instanceDecl;
} InstanceBinding;

// Copies `expr`, replacing `T.method` with the method bound by T's instance.
static AstExpr *CloneAndSpecializeExpr(Arena *arena, const AstExpr *expr, const InstanceBinding *bindings,
									   size_t bindingCount) {
	if (!expr)
		return nullptr;

	switch (expr->kind) {
		case AST_EXPR_FIELD_ACCESS: {
			if (expr->fieldAccess.target->kind == AST_EXPR_IDENT) {
				const char *targetName = expr->fieldAccess.target->identName;

				for (size_t i = 0; i < bindingCount; i++) {
					if (bindings[i].instanceDecl && strcmp(targetName, bindings[i].typeParam) == 0) {
						const AstExpr *method =
								FindInstanceMethod(bindings[i].instanceDecl, expr->fieldAccess.fieldName);
						if (method) {
							return CloneAndSpecializeExpr(arena, method, bindings, bindingCount);
						}
					}
				}
			}
			return CreateFieldAccessExpr(
					arena, CloneAndSpecializeExpr(arena, expr->fieldAccess.target, bindings, bindingCount),
					expr->fieldAccess.fieldName, expr->line, expr->column);
		}

		case AST_EXPR_CALL:
			return CreateCallExpr(arena, CloneAndSpecializeExpr(arena, expr->call.callee, bindings, bindingCount),
								  CloneAndSpecializeExpr(arena, expr->call.argument, bindings, bindingCount),
								  expr->line, expr->column);

		case AST_EXPR_LET:
			return CreateLetExpr(
					arena, expr->let.name, CloneAndSpecializeExpr(arena, expr->let.value, bindings, bindingCount),
					CloneAndSpecializeExpr(arena, expr->let.body, bindings, bindingCount), expr->line, expr->column);

		case AST_EXPR_STRUCT_VALUE: {
			const size_t   count  = expr->structValue.fieldCount;
			AstFieldValue *fields = ARENA_ARRAY(arena, AstFieldValue, count);
			for (size_t i = 0; i < count; i++) {
				fields[i].name = expr->structValue.fields[i].name;
				fields[i].value =
						CloneAndSpecializeExpr(arena, expr->structValue.fields[i].value, bindings, bindingCount);
			}
			return CreateStructValueExpr(arena, fields, count, expr->line, expr->column);
		}

		case AST_EXPR_INT_LIT:
			return CreateIntLitExpr(arena, expr->intVal, expr->line, expr->column);
		case AST_EXPR_FLOAT_LIT:
			return CreateFloatLitExpr(arena, expr->floatVal, expr->line, expr->column);
		case AST_EXPR_BOOL_LIT:
			return CreateBoolLitExpr(arena, expr->boolVal, expr->line, expr->column);
		case AST_EXPR_STRING_LIT:
			return CreateStringLitExpr(arena, expr->stringVal, expr->line, expr->column);
		case AST_EXPR_IDENT:
			return CreateIdentExpr(arena, expr->identName, expr->line, expr->column);
		case AST_EXPR_TYPE:
			return CreateTypeExpr(arena, CloneAstType(arena, expr->typeExpr), expr->line, expr->column);
		case AST_EXPR_COMPTIME:
			return CreateComptimeExpr(arena,
									  CloneAndSpecializeExpr(arena, expr->comptime.inner, bindings, bindingCount),
									  expr->line, expr->column);
	}

	return nullptr;
}

static bool DeduceTypeParam(const AstType *astParamType, LanceType *concreteType, Table *typeParamMap,
							const char **conflict) {
	if (!astParamType || !concreteType)
		return true;

	switch (astParamType->kind) {
		case AST_TYPE_NAMED: {
			const char *name = astParamType->named.name;
			if (GetPrimitiveTypeByName(name))
				return true;
			const LanceType *previous = TableGet(typeParamMap, name);
			if (previous && !TypesAreEqual(previous, concreteType)) {
				*conflict = name;
				return false;
			}
			TableSet(typeParamMap, name, concreteType);
			return true;
		}

		case AST_TYPE_LAZY:
			return DeduceTypeParam(astParamType->lazy.inner, concreteType, typeParamMap, conflict);

		case AST_TYPE_FUNCTION:
			if (concreteType->kind != TYPE_FUNCTION)
				return true;
			return DeduceTypeParam(astParamType->function.paramType, concreteType->function.paramType, typeParamMap,
								   conflict) &&
				   DeduceTypeParam(astParamType->function.returnType, concreteType->function.returnType, typeParamMap,
								   conflict);

		default:
			return true;
	}
}

// Builds the interned name `name$T1$T2...` of a specialization.
static const char *SpecializationName(const char *name, const AstConstraint *constraints, size_t count,
									  Table *typeParamMap) {
	VEC(char) buffer = {0};

	const size_t nameLength = strlen(name);
	VEC_RESERVE(buffer, nameLength);
	memcpy(buffer.items, name, nameLength);
	buffer.count = nameLength;

	for (size_t i = 0; i < count; i++) {
		const char	*typeName	= TypeToString((LanceType *) TableGet(typeParamMap, constraints[i].typeParam));
		const size_t typeLength = strlen(typeName);
		VEC_RESERVE(buffer, buffer.count + 1 + typeLength);
		buffer.items[buffer.count++] = '$';
		memcpy(buffer.items + buffer.count, typeName, typeLength);
		buffer.count += typeLength;
	}

	const char *interned = InternString(buffer.items, (uint32_t) buffer.count);
	VEC_FREE(buffer);
	return interned;
}

const Symbol *SpecializeGenericFunction(Compiler *compiler, const Symbol *symbol, TypedExpr **loweredArgs,
										const size_t argCount, const uint32_t line, const uint32_t column) {
	const AstDecl *valueDecl = symbol->valueDecl;
	if (!valueDecl) {
		CompilerError(compiler, line, column, "Generic function '%s' has no definition", symbol->name);
		return nullptr;
	}

	const AstType		*constrained	 = symbol->generic;
	const AstType		*targetType		 = constrained->constrained.targetType;
	const size_t		 constraintCount = constrained->constrained.constraintCount;
	const AstConstraint *constraints	 = constrained->constrained.constraints;

	Table typeParamMap;
	TableInit(&typeParamMap);

	const AstType *signature = targetType;
	const char	  *conflict	 = nullptr;
	for (size_t i = 0; i < argCount && signature && signature->kind == AST_TYPE_FUNCTION; i++) {
		if (!DeduceTypeParam(signature->function.paramType, loweredArgs[i]->type, &typeParamMap, &conflict)) {
			CompilerError(compiler, line, column, "Type parameter '%s' of '%s' cannot be both '%s' and '%s'", conflict,
						  symbol->name, TypeToString(TableGet(&typeParamMap, conflict)),
						  TypeToString(loweredArgs[i]->type));
			TableFree(&typeParamMap);
			return nullptr;
		}
		signature = signature->function.returnType;
	}

	const CompilerModule *callerModule	 = compiler->module;
	const CompilerModule *templateModule = ModuleOfDecl(compiler, valueDecl);
	const Symbol		 *result		 = nullptr;
	InstanceBinding		 *bindings		 = ALLOCATE(InstanceBinding, constraintCount);

	for (size_t i = 0; i < constraintCount; i++) {
		const char		*typeParam	  = constraints[i].typeParam;
		const LanceType *concreteType = (LanceType *) TableGet(&typeParamMap, typeParam);

		if (!concreteType) {
			CompilerError(compiler, line, column, "Could not deduce type parameter '%s' for generic function '%s'",
						  typeParam, symbol->name);
			goto cleanup;
		}

		if (!constraints[i].interfaceName) {
			bindings[i] = (InstanceBinding) {.typeParam = typeParam, .instanceDecl = nullptr};
			continue;
		}

		EnterModule(compiler, templateModule);
		const AstDecl *instance = LookupInstance(compiler, constraints[i].interfaceName, concreteType);
		EnterModule(compiler, callerModule);
		if (!instance) {
			CompilerError(compiler, line, column, "Type '%s' does not implement interface '%s'",
						  TypeToString(concreteType), constraints[i].interfaceName);
			goto cleanup;
		}
		bindings[i] = (InstanceBinding) {.typeParam = typeParam, .instanceDecl = instance};
	}

	EnterModule(compiler, templateModule);
	const char *specName = SpecializationName(symbol->name, constraints, constraintCount, &typeParamMap);
	result				 = SymbolTableLookupCurrentScope(compiler->globals, specName);
	if (result)
		goto cleanup;

	SymbolTable *instScope = CreateSymbolTable(compiler->globals);
	for (size_t i = 0; i < constraintCount; i++) {
		LanceType *concreteType = (LanceType *) TableGet(&typeParamMap, constraints[i].typeParam);
		SymbolTableInsert(instScope, constraints[i].typeParam, SYMBOL_TYPE, concreteType, nullptr);
	}
	LanceType *specSignature = ResolveAstType(compiler, targetType, instScope);
	FreeSymbolTable(instScope);
	if (!specSignature)
		goto cleanup;

	// Registered before lowering the body so the specialization can call itself.
	SymbolTableInsert(compiler->globals, specName, SYMBOL_VALUE, specSignature, nullptr);
	const Symbol *specialization = SymbolTableLookupCurrentScope(compiler->globals, specName);

	AstExpr *specializedBody =
			CloneAndSpecializeExpr(&compiler->specializations, valueDecl->body, bindings, constraintCount);
	if (LowerBinding(compiler, specialization->globalName, valueDecl->params, valueDecl->paramCount, specSignature,
					 specializedBody, valueDecl->line, valueDecl->column)) {
		result = specialization;
	}

cleanup:
	TableFree(&typeParamMap);
	FREE_ARRAY(InstanceBinding, bindings, constraintCount);
	EnterModule(compiler, callerModule);
	return result;
}

void AppendTypedDecl(TypedModule *module, const TypedDecl decl) {
	if (module->count >= module->capacity) {
		const size_t oldCap	 = module->capacity;
		module->capacity	 = GROW_CAPACITY(oldCap);
		module->declarations = GROW_ARRAY(TypedDecl, module->declarations, oldCap, module->capacity);
	}
	module->declarations[module->count++] = decl;
}
