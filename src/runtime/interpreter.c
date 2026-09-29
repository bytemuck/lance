#include "interpreter.h"
#include "diag.h"
#include "memory.h"
#include "primitives.h"
#include "string_pool.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

[[gnu::format(printf, 2, 3)]]
static void RuntimeError(Interpreter* interp, const char* format, ...) {
    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    ReportError("Runtime Error", SOURCE_LOC(nullptr, 0, 0), "%s", message);
    interp->hadError = true;
}

// ---- Environments ------------------------------------------------------------

static Environment* CreateEnvironment(Environment* parent) {
    Environment* env = ALLOCATE(Environment, 1);
    env->parent = parent;
    TableInit(&env->table);
    return env;
}

static void EnvironmentDefine(Environment* env, const char* name, Value* value) {
    const char* interned = InternCString(name);
    Value* existing = (Value*)TableGet(&env->table, interned);
    if (existing) {
        FreeValue(existing);
    }
    TableSet(&env->table, interned, value);
}

static Value* EnvironmentLookup(const Environment* env, const char* name) {
    const char* interned = InternCString(name);
    for (const Environment* current = env; current; current = current->parent) {
        Value* value = (Value*)TableGet(&current->table, interned);
        if (value) return value;
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

// ---- Native functions --------------------------------------------------------

// One implementation per primitive operator, generated from primitives.h.
// NUMERIC and INTEGRAL operators return their operand type, COMPARISON
// operators return bool. Integer division by zero is a runtime error.
#define DEFINE_PRIMITIVE_NUMERIC(ID, NAME, OP)                                              \
    static Value* Native##ID(Interpreter* interp, size_t argc, Value** args) {              \
        (void)argc;                                                                         \
        if (args[0]->kind == VAL_INT && args[1]->kind == VAL_INT) {                         \
            if ((PRIMITIVE_OP_##ID == PRIMITIVE_OP_Div || PRIMITIVE_OP_##ID == PRIMITIVE_OP_IntDiv) && \
                args[1]->intVal == 0) {                                                     \
                RuntimeError(interp, "Division by zero in '%s'", NAME);                     \
                return nullptr;                                                             \
            }                                                                               \
            return MakeIntValue(args[0]->intVal OP args[1]->intVal);                        \
        }                                                                                   \
        if (args[0]->kind == VAL_FLOAT && args[1]->kind == VAL_FLOAT)                       \
            return MakeFloatValue(args[0]->floatVal OP args[1]->floatVal);                  \
        RuntimeError(interp, "Invalid operands for '%s'", NAME);                            \
        return nullptr;                                                                     \
    }

#define DEFINE_PRIMITIVE_INTEGRAL(ID, NAME, OP) DEFINE_PRIMITIVE_NUMERIC(ID, NAME, OP)

#define DEFINE_PRIMITIVE_COMPARISON(ID, NAME, OP)                                           \
    static Value* Native##ID(Interpreter* interp, size_t argc, Value** args) {              \
        (void)argc;                                                                         \
        if (args[0]->kind == VAL_INT && args[1]->kind == VAL_INT)                           \
            return MakeBoolValue(args[0]->intVal OP args[1]->intVal);                       \
        if (args[0]->kind == VAL_FLOAT && args[1]->kind == VAL_FLOAT)                       \
            return MakeBoolValue(args[0]->floatVal OP args[1]->floatVal);                   \
        if (args[0]->kind == VAL_BOOL && args[1]->kind == VAL_BOOL)                         \
            return MakeBoolValue(args[0]->boolVal OP args[1]->boolVal);                     \
        RuntimeError(interp, "Invalid operands for '%s'", NAME);                            \
        return nullptr;                                                                     \
    }

#define DEFINE_PRIMITIVE(ID, NAME, CLASS, OP) DEFINE_PRIMITIVE_##CLASS(ID, NAME, OP)
LANCE_PRIMITIVE_OPERATORS(DEFINE_PRIMITIVE)
#undef DEFINE_PRIMITIVE

static const NativeFn kPrimitiveImplementations[PRIMITIVE_OP_COUNT] = {
#define PRIMITIVE_IMPLEMENTATION(ID, NAME, CLASS, OP) [PRIMITIVE_OP_##ID] = Native##ID,
    LANCE_PRIMITIVE_OPERATORS(PRIMITIVE_IMPLEMENTATION)
#undef PRIMITIVE_IMPLEMENTATION
};

static Value* NativePrint(Interpreter* interp, size_t argc, Value** args) {
    (void)interp;
    if (argc > 0 && args[0]) {
        if (args[0]->kind == VAL_STRING) {
            printf("%s\n", args[0]->stringVal);
        } else {
            PrintValue(args[0]);
            printf("\n");
        }
    }
    return MakeUnitValue();
}

// ---- Evaluation --------------------------------------------------------------

static Value* EvalTypedExpr(Interpreter* interp, const TypedExpr* expr, Environment* env);

// Evaluates a global constant on first use, so constants may refer to ones
// declared later in the file.
static Value* ForceGlobal(Interpreter* interp, const char* name) {
    const char* interned = InternCString(name);
    const TypedDecl* decl = (const TypedDecl*)TableGet(&interp->pendingGlobals, interned);
    if (!decl) return nullptr;

    if (TableGet(&interp->evaluatingGlobals, interned)) {
        RuntimeError(interp, "Definition of '%s' depends on itself", name);
        return nullptr;
    }

    TableSet(&interp->evaluatingGlobals, interned, (void*)decl);
    Value* value = EvalTypedExpr(interp, decl->body, interp->globals);
    TableDelete(&interp->evaluatingGlobals, interned);
    TableDelete(&interp->pendingGlobals, interned);

    if (!value) return nullptr;
    EnvironmentDefine(interp->globals, interned, value);
    return value;
}

static Value* LookupVariable(Interpreter* interp, Environment* env, const char* name) {
    Value* value = EnvironmentLookup(env, name);
    if (!value) value = ForceGlobal(interp, name);
    if (!value && !interp->hadError) {
        RuntimeError(interp, "Undefined variable '%s'", name);
    }
    return value;
}

// Arguments applied so far, plus the new one. Callables are curried: they
// collect arguments until their arity is reached.
static Value** CollectArguments(Value** applied, const size_t appliedCount, const Value* arg) {
    Value** args = ALLOCATE(Value*, appliedCount + 1);
    for (size_t i = 0; i < appliedCount; i++) {
        args[i] = CopyValue(applied[i]);
    }
    args[appliedCount] = CopyValue(arg);
    return args;
}

static void FreeArguments(Value** args, const size_t count) {
    for (size_t i = 0; i < count; i++) FreeValue(args[i]);
    FREE_ARRAY(Value*, args, count);
}

static Value* ApplyFunction(Interpreter* interp, const Value* callee, const Value* arg) {
    Value* result = nullptr;

    if (callee->kind == VAL_NATIVE_FN) {
        const NativeFunctionValue* fn = &callee->nativeFn;
        const size_t count = fn->appliedCount + 1;
        Value** args = CollectArguments(fn->appliedArgs, fn->appliedCount, arg);

        result = count < fn->arity
            ? MakeNativeFnValue(fn->name, fn->fn, fn->arity, args, count)
            : fn->fn(interp, count, args);

        FreeArguments(args, count);
        return result;
    }

    if (callee->kind == VAL_CLOSURE) {
        const ClosureValue* closure = &callee->closure;
        const size_t count = closure->appliedCount + 1;
        Value** args = CollectArguments(closure->appliedArgs, closure->appliedCount, arg);

        if (count < closure->totalParams) {
            result = MakeClosureValue(closure->decl, closure->closureEnv, args, count);
        } else {
            Environment* callEnv = CreateEnvironment(closure->closureEnv);
            for (size_t i = 0; i < closure->decl->paramCount; i++) {
                EnvironmentDefine(callEnv, closure->decl->params[i], CopyValue(args[i]));
            }
            result = EvalTypedExpr(interp, closure->decl->body, callEnv);
            FreeEnvironment(callEnv);
        }

        FreeArguments(args, count);
        return result;
    }

    RuntimeError(interp, "Attempted to call a non-function value");
    return nullptr;
}

static Value* EvalFieldAccess(Interpreter* interp, const TypedExpr* expr, Environment* env) {
    Value* target = EvalTypedExpr(interp, expr->fieldAccess.target, env);
    if (!target) return nullptr;

    Value* result = nullptr;
    if (target->kind != VAL_STRUCT) {
        RuntimeError(interp, "Field access '%s' on a non-struct value", expr->fieldAccess.fieldName);
    } else if (expr->fieldAccess.fieldIndex < target->structVal.fieldCount) {
        result = CopyValue(target->structVal.fields[expr->fieldAccess.fieldIndex].value);
    }

    FreeValue(target);
    return result;
}

static Value* EvalStructInit(Interpreter* interp, const TypedExpr* expr, Environment* env) {
    const size_t count = expr->structInit.fieldCount;
    StructFieldValue* fields = ALLOCATE(StructFieldValue, count);

    for (size_t i = 0; i < count; i++) {
        fields[i].name = expr->structInit.fields[i].name;
        fields[i].value = EvalTypedExpr(interp, expr->structInit.fields[i].value, env);
    }

    const LanceType* structType = expr->structInit.structType;
    const char* typeName = structType && structType->kind == TYPE_STRUCT ? structType->structType.name : nullptr;
    Value* result = MakeStructValue(typeName, fields, count);

    if (interp->hadError) {
        FreeValue(result);
        return nullptr;
    }
    return result;
}

static Value* EvalCall(Interpreter* interp, const TypedExpr* expr, Environment* env) {
    Value* callee = EvalTypedExpr(interp, expr->call.callee, env);
    Value* arg = callee ? EvalTypedExpr(interp, expr->call.argument, env) : nullptr;

    Value* result = callee && arg ? ApplyFunction(interp, callee, arg) : nullptr;

    FreeValue(callee);
    FreeValue(arg);
    return result;
}

static Value* EvalTypedExpr(Interpreter* interp, const TypedExpr* expr, Environment* env) {
    if (!expr || interp->hadError) return nullptr;

    switch (expr->kind) {
        case TYPED_EXPR_INT_LIT:      return MakeIntValue(expr->intVal);
        case TYPED_EXPR_FLOAT_LIT:    return MakeFloatValue(expr->floatVal);
        case TYPED_EXPR_BOOL_LIT:     return MakeBoolValue(expr->boolVal);
        case TYPED_EXPR_STRING_LIT:   return MakeStringValue(expr->stringVal);
        case TYPED_EXPR_VAR:          return CopyValue(LookupVariable(interp, env, expr->varName));
        case TYPED_EXPR_FIELD_ACCESS: return EvalFieldAccess(interp, expr, env);
        case TYPED_EXPR_STRUCT_INIT:  return EvalStructInit(interp, expr, env);
        case TYPED_EXPR_CALL:         return EvalCall(interp, expr, env);
    }

    return nullptr;
}

// ---- Entry points ------------------------------------------------------------

void InitializeInterpreter(Interpreter* interp) {
    interp->globals = CreateEnvironment(nullptr);
    interp->module = nullptr;
    interp->hadError = false;
    TableInit(&interp->pendingGlobals);
    TableInit(&interp->evaluatingGlobals);

    for (size_t i = 0; i < PRIMITIVE_OP_COUNT; i++) {
        const char* name = kPrimitiveOperators[i].name;
        EnvironmentDefine(interp->globals, name,
                          MakeNativeFnValue(name, kPrimitiveImplementations[i], 2, nullptr, 0));
    }
    EnvironmentDefine(interp->globals, LANCE_PRINT_NAME,
                      MakeNativeFnValue(LANCE_PRINT_NAME, NativePrint, 1, nullptr, 0));
}

void FreeInterpreter(Interpreter* interp) {
    FreeEnvironment(interp->globals);
    interp->globals = nullptr;
    TableFree(&interp->pendingGlobals);
    TableFree(&interp->evaluatingGlobals);
}

Value* InterpretTypedModule(Interpreter* interp, const TypedModule* module) {
    if (!module) return nullptr;

    interp->module = module;

    // Functions become closures right away; constants are evaluated lazily.
    for (size_t i = 0; i < module->count; i++) {
        const TypedDecl* decl = &module->declarations[i];
        if (decl->paramCount > 0) {
            EnvironmentDefine(interp->globals, decl->name, MakeClosureValue(decl, interp->globals, nullptr, 0));
        } else {
            TableSet(&interp->pendingGlobals, InternCString(decl->name), (void*)decl);
        }
    }

    // Force the remaining constants in source order, so their effects (such as
    // `print`) happen in a predictable order.
    for (size_t i = 0; i < module->count && !interp->hadError; i++) {
        const TypedDecl* decl = &module->declarations[i];
        if (decl->paramCount == 0) ForceGlobal(interp, decl->name);
    }

    if (interp->hadError) return nullptr;

    const Value* mainValue = EnvironmentLookup(interp->globals, "main");
    return mainValue ? CopyValue(mainValue) : nullptr;
}
