#include "compiler_internal.h"
#include "string_pool.h"

#include <string.h>

void RegisterInterface(Compiler* compiler, const AstDecl* definition) {
    if (!definition || definition->paramCount != 1 || !definition->body ||
        definition->body->kind != AST_EXPR_TYPE || !definition->body->typeExpr ||
        definition->body->typeExpr->kind != AST_TYPE_STRUCT) {
        return;
    }

    VEC_PUSH(compiler->interfaces, ((CompilerInterface){
        .name = InternCString(definition->name),
        .typeParam = definition->params[0],
        .methods = definition->body->typeExpr,
    }));
}

const CompilerInterface* LookupInterface(const Compiler* compiler, const char* name) {
    const char* interned = InternCString(name);
    for (size_t i = 0; i < compiler->interfaces.count; i++) {
        if (compiler->interfaces.items[i].name == interned) return &compiler->interfaces.items[i];
    }
    return nullptr;
}

void RegisterInstance(Compiler* compiler, const char* interfaceName, const LanceType* type, const AstDecl* decl) {
    VEC_PUSH(compiler->instances, ((CompilerInstance){
        .interfaceName = InternCString(interfaceName),
        .targetType = type,
        .instanceDecl = decl,
    }));
}

const AstDecl* LookupInstance(const Compiler* compiler, const char* interfaceName, const LanceType* type) {
    const char* interned = InternCString(interfaceName);
    for (size_t i = 0; i < compiler->instances.count; i++) {
        const CompilerInstance* instance = &compiler->instances.items[i];
        if (instance->interfaceName == interned && TypesAreEqual(instance->targetType, type)) {
            return instance->instanceDecl;
        }
    }
    return nullptr;
}

const AstExpr* FindInstanceMethod(const AstDecl* instanceDecl, const char* methodName) {
    if (!instanceDecl || !instanceDecl->body || instanceDecl->body->kind != AST_EXPR_STRUCT_VALUE) {
        return nullptr;
    }

    const size_t fieldCount = instanceDecl->body->structValue.fieldCount;
    const AstFieldValue* fields = instanceDecl->body->structValue.fields;
    for (size_t i = 0; i < fieldCount; i++) {
        if (strcmp(fields[i].name, methodName) == 0) {
            return fields[i].value;
        }
    }
    return nullptr;
}

static const AstType* FindMethodSignature(const CompilerInterface* interface, const char* methodName) {
    if (!interface) return nullptr;
    for (size_t i = 0; i < interface->methods->structType.fieldCount; i++) {
        if (strcmp(interface->methods->structType.fields[i].name, methodName) == 0) {
            return interface->methods->structType.fields[i].type;
        }
    }
    return nullptr;
}

InstanceMethodLookup LookupInstanceMethodForType(Compiler* compiler, const LanceType* targetType, const char* methodName) {
    InstanceMethodLookup result = {0};
    if (!targetType || !methodName) return result;

    for (size_t i = 0; i < compiler->instances.count; i++) {
        const CompilerInstance* instance = &compiler->instances.items[i];
        if (!TypesAreEqual(instance->targetType, targetType)) continue;

        const AstExpr* method = FindInstanceMethod(instance->instanceDecl, methodName);
        if (!method) continue;

        result.instanceDecl = instance->instanceDecl;
        result.methodExpr = method;

        const CompilerInterface* interface = LookupInterface(compiler, instance->interfaceName);
        const AstType* signature = FindMethodSignature(interface, methodName);
        if (signature) {
            result.methodType = InstantiateGenericType(compiler, signature, interface->typeParam,
                                                       targetType, compiler->globals);
        }
        return result;
    }
    return result;
}

bool HasInstanceMethod(const Compiler* compiler, const char* methodName) {
    for (size_t i = 0; i < compiler->instances.count; i++) {
        if (FindInstanceMethod(compiler->instances.items[i].instanceDecl, methodName)) {
            return true;
        }
    }
    return false;
}
