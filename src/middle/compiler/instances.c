#include "compiler_internal.h"

#include <string.h>

void RegisterInterface(Compiler *compiler, const AstDecl *definition) {
	if (!definition || definition->paramCount != 1 || !definition->body || definition->body->kind != AST_EXPR_TYPE ||
		!definition->body->typeExpr || definition->body->typeExpr->kind != AST_TYPE_STRUCT) {
		return;
	}

	VEC_PUSH(compiler->interfaces, ((CompilerInterface) {
										   .symbol = SymbolTableLookupCurrentScope(compiler->globals, definition->name),
										   .typeParam = definition->params[0],
										   .methods	  = definition->body->typeExpr,
										   .module	  = compiler->module,
								   }));
}

static const CompilerInterface *FindInterface(const Compiler *compiler, const Symbol *symbol) {
	for (size_t i = 0; i < compiler->interfaces.count; i++) {
		if (compiler->interfaces.items[i].symbol == symbol)
			return &compiler->interfaces.items[i];
	}
	return nullptr;
}

// An interface is visible where its name resolves to its own symbol.
static bool IsInterfaceVisible(const Compiler *compiler, const Symbol *interface) {
	return SymbolTableLookup(compiler->globals, interface->name) == interface;
}

void RegisterInstance(Compiler *compiler, const Symbol *interface, const LanceType *type, const AstDecl *decl) {
	VEC_PUSH(compiler->instances, ((CompilerInstance) {
										  .interface	= interface,
										  .targetType	= type,
										  .instanceDecl = decl,
										  .module		= compiler->module,
								  }));
}

const AstDecl *LookupInstance(const Compiler *compiler, const char *interfaceName, const LanceType *type) {
	const Symbol *interface = SymbolTableLookup(compiler->globals, interfaceName);
	if (!interface)
		return nullptr;

	for (size_t i = 0; i < compiler->instances.count; i++) {
		const CompilerInstance *instance = &compiler->instances.items[i];
		if (instance->interface == interface && TypesAreEqual(instance->targetType, type)) {
			return instance->instanceDecl;
		}
	}
	return nullptr;
}

const AstExpr *FindInstanceMethod(const AstDecl *instanceDecl, const char *methodName) {
	if (!instanceDecl || !instanceDecl->body || instanceDecl->body->kind != AST_EXPR_STRUCT_VALUE) {
		return nullptr;
	}

	const size_t		 fieldCount = instanceDecl->body->structValue.fieldCount;
	const AstFieldValue *fields		= instanceDecl->body->structValue.fields;
	for (size_t i = 0; i < fieldCount; i++) {
		if (strcmp(fields[i].name, methodName) == 0) {
			return fields[i].value;
		}
	}
	return nullptr;
}

static const AstType *FindMethodSignature(const CompilerInterface *interface, const char *methodName) {
	if (!interface)
		return nullptr;
	for (size_t i = 0; i < interface->methods->structType.fieldCount; i++) {
		if (strcmp(interface->methods->structType.fields[i].name, methodName) == 0) {
			return interface->methods->structType.fields[i].type;
		}
	}
	return nullptr;
}

InstanceMethodLookup LookupInstanceMethodForType(Compiler *compiler, const LanceType *targetType,
												 const char *methodName) {
	InstanceMethodLookup result = {0};
	if (!targetType || !methodName)
		return result;

	for (size_t i = 0; i < compiler->instances.count; i++) {
		const CompilerInstance *instance = &compiler->instances.items[i];
		if (!TypesAreEqual(instance->targetType, targetType) || !IsInterfaceVisible(compiler, instance->interface)) {
			continue;
		}

		const AstExpr *method = FindInstanceMethod(instance->instanceDecl, methodName);
		if (!method)
			continue;

		result.instanceDecl = instance->instanceDecl;
		result.methodExpr	= method;
		result.module		= instance->module;

		const CompilerInterface *interface = FindInterface(compiler, instance->interface);
		const AstType			*signature = FindMethodSignature(interface, methodName);
		if (signature) {
			result.methodType = InstantiateGenericType(compiler, signature, interface->typeParam, targetType,
													   interface->module->scope);
		}
		return result;
	}
	return result;
}

bool HasInstanceMethod(const Compiler *compiler, const char *methodName) {
	for (size_t i = 0; i < compiler->instances.count; i++) {
		const CompilerInstance *instance = &compiler->instances.items[i];
		if (IsInterfaceVisible(compiler, instance->interface) &&
			FindInstanceMethod(instance->instanceDecl, methodName)) {
			return true;
		}
	}
	return false;
}
