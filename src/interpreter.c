#include "interpreter.h"
#include "memory.h"
#include "string_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Environment* CreateEnvironment(Environment* parent) {
    Environment* env = ALLOCATE(Environment, 1);
    if (!env) return nullptr;

    env->parent = parent;
    TableInit(&env->table);
    return env;
}

static void EnvironmentDefine(Environment* env, const char* name, Value* value) {
    if (!env || !name) return;

    const char* interned = InternCString(name);
    Value* existing = (Value*)TableGet(&env->table, interned);
    if (existing) {
        FreeValue(existing);
    }
    TableSet(&env->table, interned, value);
}

static Value* EnvironmentLookup(const Environment* env, const char* name) {
    if (!name) return nullptr;

    const char* interned = InternCString(name);
    const Environment* current = env;
    while (current != nullptr) {
        Value* val = (Value*)TableGet(&current->table, interned);
        if (val != nullptr) {
            return val;
        }
        current = current->parent;
    }
    return nullptr;
}

static void FreeEnvironment(Environment* env) {
    if (!env) return;
    for (size_t i = 0; i < env->table.capacity; i++) {
        TableEntry* entry = &env->table.entries[i];
        if (entry->key != nullptr && entry->value != nullptr && entry->value != (void*)1) {
            FreeValue((Value*)entry->value);
        }
    }
    TableFree(&env->table);
    FREE(Environment, env);
}

static Value* EvalTypedExpr(Interpreter* interp, const TypedExpr* expr, Environment* env);

static Value* NativeAdd(Interpreter* interp, size_t argc, Value** args) {
    if (args[0]->kind == VAL_INT && args[1]->kind == VAL_INT)
        return MakeIntValue(args[0]->intVal + args[1]->intVal);
    if (args[0]->kind == VAL_FLOAT && args[1]->kind == VAL_FLOAT)
        return MakeFloatValue(args[0]->floatVal + args[1]->floatVal);
    return MakeUnitValue();
}

static Value* NativeSub(Interpreter* interp, size_t argc, Value** args) {
    if (args[0]->kind == VAL_INT && args[1]->kind == VAL_INT)
        return MakeIntValue(args[0]->intVal - args[1]->intVal);
    if (args[0]->kind == VAL_FLOAT && args[1]->kind == VAL_FLOAT)
        return MakeFloatValue(args[0]->floatVal - args[1]->floatVal);
    return MakeUnitValue();
}

static Value* NativeMul(Interpreter* interp, size_t argc, Value** args) {
    if (args[0]->kind == VAL_INT && args[1]->kind == VAL_INT)
        return MakeIntValue(args[0]->intVal * args[1]->intVal);
    if (args[0]->kind == VAL_FLOAT && args[1]->kind == VAL_FLOAT)
        return MakeFloatValue(args[0]->floatVal * args[1]->floatVal);
    return MakeUnitValue();
}

static Value* NativeDiv(Interpreter* interp, size_t argc, Value** args) {
    if (args[0]->kind == VAL_INT && args[1]->kind == VAL_INT)
        return MakeIntValue(args[1]->intVal != 0 ? args[0]->intVal / args[1]->intVal : 0);
    if (args[0]->kind == VAL_FLOAT && args[1]->kind == VAL_FLOAT)
        return MakeFloatValue(args[1]->floatVal != 0 ? args[0]->floatVal / args[1]->floatVal : 0.0);
    return MakeUnitValue();
}

static Value* NativeEq(Interpreter* interp, size_t argc, Value** args) {
    if (args[0]->kind == VAL_INT && args[1]->kind == VAL_INT)
        return MakeBoolValue(args[0]->intVal == args[1]->intVal);
    if (args[0]->kind == VAL_FLOAT && args[1]->kind == VAL_FLOAT)
        return MakeBoolValue(args[0]->floatVal == args[1]->floatVal);
    if (args[0]->kind == VAL_BOOL && args[1]->kind == VAL_BOOL)
        return MakeBoolValue(args[0]->boolVal == args[1]->boolVal);
    return MakeBoolValue(false);
}

static Value* NativeNeq(Interpreter* interp, size_t argc, Value** args) {
    if (args[0]->kind == VAL_INT && args[1]->kind == VAL_INT)
        return MakeBoolValue(args[0]->intVal != args[1]->intVal);
    if (args[0]->kind == VAL_FLOAT && args[1]->kind == VAL_FLOAT)
        return MakeBoolValue(args[0]->floatVal != args[1]->floatVal);
    if (args[0]->kind == VAL_BOOL && args[1]->kind == VAL_BOOL)
        return MakeBoolValue(args[0]->boolVal != args[1]->boolVal);
    return MakeBoolValue(true);
}

static Value* NativeLt(Interpreter* interp, size_t argc, Value** args) {
    if (args[0]->kind == VAL_INT && args[1]->kind == VAL_INT)
        return MakeBoolValue(args[0]->intVal < args[1]->intVal);
    if (args[0]->kind == VAL_FLOAT && args[1]->kind == VAL_FLOAT)
        return MakeBoolValue(args[0]->floatVal < args[1]->floatVal);
    return MakeBoolValue(false);
}

static Value* NativeLte(Interpreter* interp, size_t argc, Value** args) {
    if (args[0]->kind == VAL_INT && args[1]->kind == VAL_INT)
        return MakeBoolValue(args[0]->intVal <= args[1]->intVal);
    if (args[0]->kind == VAL_FLOAT && args[1]->kind == VAL_FLOAT)
        return MakeBoolValue(args[0]->floatVal <= args[1]->floatVal);
    return MakeBoolValue(false);
}

static Value* NativeGt(Interpreter* interp, size_t argc, Value** args) {
    if (args[0]->kind == VAL_INT && args[1]->kind == VAL_INT)
        return MakeBoolValue(args[0]->intVal > args[1]->intVal);
    if (args[0]->kind == VAL_FLOAT && args[1]->kind == VAL_FLOAT)
        return MakeBoolValue(args[0]->floatVal > args[1]->floatVal);
    return MakeBoolValue(false);
}

static Value* NativeGte(Interpreter* interp, size_t argc, Value** args) {
    if (args[0]->kind == VAL_INT && args[1]->kind == VAL_INT)
        return MakeBoolValue(args[0]->intVal >= args[1]->intVal);
    if (args[0]->kind == VAL_FLOAT && args[1]->kind == VAL_FLOAT)
        return MakeBoolValue(args[0]->floatVal >= args[1]->floatVal);
    return MakeBoolValue(false);
}

static Value* ApplyFunction(Interpreter* interp, Value* callee, Value* arg) {
    if (!callee) return nullptr;

    if (callee->kind == VAL_NATIVE_FN) {
        size_t newCount = callee->nativeFn.appliedCount + 1;
        Value** newArgs = (Value**)malloc(newCount * sizeof(Value*));

        for (size_t i = 0; i < callee->nativeFn.appliedCount; i++) {
            newArgs[i] = CopyValue(callee->nativeFn.appliedArgs[i]);
        }
        newArgs[newCount - 1] = CopyValue(arg);

        if (newCount < callee->nativeFn.arity) {
            Value* partial = MakeNativeFnValue(callee->nativeFn.name, callee->nativeFn.fn,
                                               callee->nativeFn.arity, newArgs, newCount);
            for (size_t i = 0; i < newCount; i++) FreeValue(newArgs[i]);
            free(newArgs);
            return partial;
        }

        Value* result = callee->nativeFn.fn(interp, newCount, newArgs);
        for (size_t i = 0; i < newCount; i++) FreeValue(newArgs[i]);
        free(newArgs);
        return result;
    }

    if (callee->kind == VAL_CLOSURE) {
        size_t newCount = callee->closure.appliedCount + 1;
        Value** newArgs = (Value**)malloc(newCount * sizeof(Value*));

        for (size_t i = 0; i < callee->closure.appliedCount; i++) {
            newArgs[i] = CopyValue(callee->closure.appliedArgs[i]);
        }
        newArgs[newCount - 1] = CopyValue(arg);

        if (newCount < callee->closure.totalParams) {
            Value* partial = MakeClosureValue(callee->closure.decl, callee->closure.closureEnv,
                                              newArgs, newCount);
            for (size_t i = 0; i < newCount; i++) FreeValue(newArgs[i]);
            free(newArgs);
            return partial;
        }

        Environment* callEnv = CreateEnvironment(callee->closure.closureEnv);
        const TypedDecl* decl = callee->closure.decl;

        for (size_t i = 0; i < decl->paramCount; i++) {
            EnvironmentDefine(callEnv, decl->params[i], CopyValue(newArgs[i]));
        }

        Value* result = EvalTypedExpr(interp, decl->body, callEnv);

        FreeEnvironment(callEnv);
        for (size_t i = 0; i < newCount; i++) FreeValue(newArgs[i]);
        free(newArgs);
        return result;
    }

    return nullptr;
}

static Value* EvalTypedExpr(Interpreter* interp, const TypedExpr* expr, Environment* env) {
    if (!expr) return nullptr;

    switch (expr->kind) {
        case TYPED_EXPR_INT_LIT:
            return MakeIntValue(expr->intVal);

        case TYPED_EXPR_FLOAT_LIT:
            return MakeFloatValue(expr->floatVal);

        case TYPED_EXPR_BOOL_LIT:
            return MakeBoolValue(expr->boolVal);

        case TYPED_EXPR_STRING_LIT:
            return MakeStringValue(expr->stringVal);

        case TYPED_EXPR_VAR: {
            Value* val = EnvironmentLookup(env, expr->varName);
            if (val) return CopyValue(val);
            return nullptr;
        }

        case TYPED_EXPR_FIELD_ACCESS: {
            Value* target = EvalTypedExpr(interp, expr->fieldAccess.target, env);
            if (!target || target->kind != VAL_STRUCT) {
                FreeValue(target);
                return nullptr;
            }

            Value* result = nullptr;
            if (expr->fieldAccess.fieldIndex < target->structVal.fieldCount) {
                result = CopyValue(target->structVal.fields[expr->fieldAccess.fieldIndex].value);
            } else {
                for (size_t i = 0; i < target->structVal.fieldCount; i++) {
                    if (strcmp(target->structVal.fields[i].name, expr->fieldAccess.fieldName) == 0) {
                        result = CopyValue(target->structVal.fields[i].value);
                        break;
                    }
                }
            }

            FreeValue(target);
            return result;
        }

        case TYPED_EXPR_STRUCT_INIT: {
            size_t count = expr->structInit.fieldCount;
            StructFieldValue* fields = (StructFieldValue*)malloc(count * sizeof(StructFieldValue));

            for (size_t i = 0; i < count; i++) {
                fields[i].name = expr->structInit.fields[i].name;
                fields[i].value = EvalTypedExpr(interp, expr->structInit.fields[i].value, env);
            }

            const char* typeName = nullptr;
            if (expr->structInit.structType && expr->structInit.structType->kind == TYPE_STRUCT) {
                typeName = expr->structInit.structType->structType.name;
            }

            return MakeStructValue(typeName, fields, count);
        }

        case TYPED_EXPR_CALL: {
            Value* callee = EvalTypedExpr(interp, expr->call.callee, env);
            Value* arg    = EvalTypedExpr(interp, expr->call.argument, env);

            Value* result = ApplyFunction(interp, callee, arg);

            FreeValue(callee);
            FreeValue(arg);
            return result;
        }
    }

    return nullptr;
}

void InitializeInterpreter(Interpreter* interp) {
    interp->globals = CreateEnvironment(nullptr);
    interp->module = nullptr;

    EnvironmentDefine(interp->globals, "+",  MakeNativeFnValue("+",  NativeAdd, 2, nullptr, 0));
    EnvironmentDefine(interp->globals, "-",  MakeNativeFnValue("-",  NativeSub, 2, nullptr, 0));
    EnvironmentDefine(interp->globals, "*",  MakeNativeFnValue("*",  NativeMul, 2, nullptr, 0));
    EnvironmentDefine(interp->globals, "/",  MakeNativeFnValue("/",  NativeDiv, 2, nullptr, 0));
    EnvironmentDefine(interp->globals, "==", MakeNativeFnValue("==", NativeEq,  2, nullptr, 0));
    EnvironmentDefine(interp->globals, "!=", MakeNativeFnValue("!=", NativeNeq, 2, nullptr, 0));
    EnvironmentDefine(interp->globals, "<",  MakeNativeFnValue("<",  NativeLt,  2, nullptr, 0));
    EnvironmentDefine(interp->globals, "<=", MakeNativeFnValue("<=", NativeLte, 2, nullptr, 0));
    EnvironmentDefine(interp->globals, ">",  MakeNativeFnValue(">",  NativeGt,  2, nullptr, 0));
    EnvironmentDefine(interp->globals, ">=", MakeNativeFnValue(">=", NativeGte, 2, nullptr, 0));
}

void FreeInterpreter(Interpreter* interp) {
    FreeEnvironment(interp->globals);
}

Value* InterpretTypedModule(Interpreter* interp, const TypedModule* module) {
    if (!module) return nullptr;

    interp->module = module;

    for (size_t i = 0; i < module->count; i++) {
        const TypedDecl* decl = &module->declarations[i];

        if (decl->paramCount > 0) {
            Value* closure = MakeClosureValue(decl, interp->globals, nullptr, 0);
            EnvironmentDefine(interp->globals, decl->name, closure);
        } else {
            Value* val = EvalTypedExpr(interp, decl->body, interp->globals);
            if (val) {
                EnvironmentDefine(interp->globals, decl->name, val);
            }
        }
    }

    Value* mainVal = EnvironmentLookup(interp->globals, "main");
    if (mainVal) {
        return CopyValue(mainVal);
    }

    return nullptr;
}
